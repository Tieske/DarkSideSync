#include <stdlib.h>
#include <lauxlib.h>
#include "udpsocket.h"
#include "locking.h"
#include "darksidesync.h"

static putilRecord volatile UtilStart = NULL;		// Holds first utility in the list
static void* volatile DSS_initialized = NULL;		// while its NULL, the first mutex is uninitialized
static DSS_mutex_t utillock;						// lock to protect the network counter
static int statecount = 0;							// counter for number of lua states using this lib
static DSS_mutex_t statelock;						// lock to protect the state counter
static DSS_api_1v0_t DSS_api_1v0;					// API struct for version 1.0

// forward definitions
void setUDPPort (pglobalRecord globals, int newPort);

// TODO: add a status; running, stopping, stopped, including errors when attempting to register/deliver while stopping/stopped

// TODO: find reference to functions that lock, and check if they need an 'unprotected' version

/*
** ===============================================================
** Utility and globals registration functions
** ===============================================================
*/
	// Returns 1 if the LuaState has a global struct, 0 otherwise
	int DSS_hasstateglobals(lua_State *L)
	{
		pglobalRecord globals;
		// try and collect the globals userdata
		lua_getfield(L, LUA_REGISTRYINDEX, DSS_GLOBALS_KEY);
		globals = lua_touserdata(L, -1);
		lua_pop(L,1);
		if (globals == NULL) return 0;
		return 1;
	}

	// Gets the pointer to the structure with all LuaState related globals
	// that are to be kept outside the LuaState (to be accessible from the
	// async callbacks)
	// If the structure is not found, a new one is created and initialized.
	// Locking isn't necessary, as the structure is collected from/added to
	// the Lua_State, hence the calling code must already be single threaded.
	// returns NULL upon failure
	// see also DSS_getvalidglobals()
	pglobalRecord DSS_getstateglobals(lua_State *L)
	{
		//TODO: if it fails, handle errors from calling functions properly

		pglobalRecord globals;

		// try and collect the globals userdata
		lua_getfield(L, LUA_REGISTRYINDEX, DSS_GLOBALS_KEY);
		globals = lua_touserdata(L, -1);
		lua_pop(L,1);

		if (globals == NULL)
		{
			int err = 0;
			// Not found, create a new one
			lua_pop(L, 1);	// pop the failed result
			globals = lua_newuserdata(L, sizeof(pglobalRecord));
			if (globals == NULL) err = 1;	// alloc failed

			if (err == 0) 
			{
				lua_setfield(L, -1, DSS_GLOBALS_KEY);

				// now setup UDP port and status
				(*globals).udpport = 0;
				(*globals).DSS_status = DSS_STATUS_STOPPED;

				// setup data queue
				if (DSS_mutexInit((*globals).lock) != 0) err = 1;
			}

			if (err == 0)
			{
				(*globals).QueueCount = 0;
				(*globals).QueueEnd = NULL;
				(*globals).QueueStart = NULL;
			}

			if (err != 0)	// we had an error
			{
				// remove userdata anchor
				lua_pushnil(L);
				lua_setfield(L, -1, DSS_GLOBALS_KEY);

				// report error
				return NULL;
			}

			// now add a garbagecollect metamethod to clean it up afterwards
			// TODO: add garbage collect metamethod
			
		}

		return globals;
	}

	// checks global struct, returns 1 if valid, 0 otherwise
	// TODO: should this LOCK the global record?
	int DSS_isvalidglobals(pglobalRecord g)
	{
		if (g == NULL || (*g).DSS_status != DSS_STATUS_STARTED)
		{
			return 0;	// not valid
		}
		else
		{
			return 1;	// valid
		}
	}

	// returns a valid globals struct or fails with a Lua error (and won't
	// return in that case)
	pglobalRecord DSS_getvalidglobals(lua_State *L)
	{
		pglobalRecord g = DSS_getstateglobals(L);
		if (DSS_isvalidglobals(g) == 0) 
		{
			// following error call will not return
			luaL_error(L, "DSS was not started yet, or already stopped");
		}
		return g;
	}

	void DSS_clearstateglobals(lua_State *L)
	{
		// TODO: implement GC method
		pglobalRecord globals;
		putilRecord listend;

		// If there is no state global, exit immediately, there is no cleanup to do
		if (DSS_hasstateglobals(L) == 0) return;

		globals = DSS_getstateglobals(L);

		// Set status to stopping, registering and delivering will fail from here on
		DSS_mutexLock((*globals).lock);
		(*globals).DSS_status = DSS_STATUS_STOPPING;
		DSS_mutexUnlock((*globals).lock);	// unlock to let any waiting threads move in (and fail, because we're stopping)
		

		// cancel all utilities, in reverse order
		DSS_mutexLock(utillock);
		while (UtilStart != NULL)
		{
			listend = UtilStart;
			while ((*listend).pNext != NULL) listend = (*listend).pNext;

			DSS_mutexUnlock(utillock);		// must unlock to let the cancel function succeed
			(*listend).pCancel(listend, (*listend).pUtilData);	// call this utility's cancel method
			DSS_mutexLock(utillock);		// lock again to get the next one
		}
		DSS_mutexUnlock(utillock);
		
		// update status again, we're done stopping
		DSS_mutexLock((*globals).lock);
		(*globals).DSS_status = DSS_STATUS_STOPPED;
		DSS_mutexUnlock((*globals).lock);	

		// Remove references from the registry
		lua_pushnil(L);
		lua_setfield(L, -1, DSS_GLOBALS_KEY);
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

		// Close socket and destroy mutex
		setUDPPort(globals, 0);  // set port to 0, will close socket
		DSS_mutexDestroy((*globals).lock);

		// Reduce state count and close network if none left
		DSS_mutexLock(statelock);
		statecount = statecount - 1;
		if (statecount == 0)
		{
			DSS_networkStop();
		}
		DSS_mutexUnlock(statelock);
	}

/*
** ===============================================================
** Queue management functions
** ===============================================================
*/
	// Push item in the queue
	// Returns number of items in queue, or DSS_ERR_OUT_OF_MEMORY or 
	// DSS_ERR_NOTSTARTED if it failed
	int queuePush (putilRecord utilid, DSS_decoder_1v0_t pDecode, void* pData)
	{
		int cnt;
		pqueueItem pqi = NULL;
		pglobalRecord globals = (*utilid).pGlobals;
		if (DSS_isvalidglobals(globals) == 0) return DSS_ERR_NOT_STARTED;

		if (NULL == (pqi = malloc(sizeof(queueItem))))
			return DSS_ERR_OUT_OF_MEMORY;	// exit, memory alloc failed

		(*pqi).utilid = utilid;
		(*pqi).pDecode = pDecode;
		(*pqi).pData = pData;
		(*pqi).pNext = NULL;
		(*pqi).pPrevious = NULL;

		DSS_mutexLock((*globals).lock);
		if ((*globals).QueueStart == NULL)
		{
			// first item in queue
			(*globals).QueueStart = pqi;
			(*globals).QueueEnd = pqi;
		}
		else
		{
			// append to queue
			(*(*globals).QueueEnd).pNext = pqi;
			(*pqi).pPrevious = (*globals).QueueEnd;
			(*globals).QueueEnd = pqi;
		}
		(*globals).QueueCount += 1;
		cnt = (*globals).QueueCount;
		DSS_mutexUnlock((*globals).lock);

		return cnt;
	}

	// Pop item from the queue, without locking; caller must lock!
	// Returns queueItem filled, or all NULLs if none available
	// utilid is id of utility for which to return, or NULL to
	// just return the oldest item, independent of a utilid
	queueItem queuePopUnlocked(pglobalRecord globals, putilRecord utilid)
	{
		pqueueItem qi;
		queueItem result;
		result.utilid = NULL;
		result.pData = NULL;
		result.pDecode = NULL;
		result.pNext = NULL;
		result.pPrevious = NULL;

		
		DSS_mutexLock((*globals).lock);
		if ((*globals).QueueStart != NULL)
		{

			if (utilid == NULL)
			{
				// just grab the oldest from the queue
				qi = (*globals).QueueStart;
			}
			else
			{
				// traverse backwards to find the last one with a corresponding utilid
				qi = (*globals).QueueStart;
				while (qi != NULL && (*qi).utilid != utilid)
				{
					qi = (*qi).pPrevious;
				}
			}

			if (qi != NULL) 
			{
				result = (*qi);
				// dismiss item from linked list
				if (qi == (*globals).QueueStart) (*globals).QueueStart = (*qi).pNext;
				if (qi == (*globals).QueueEnd) (*globals).QueueEnd = (*qi).pPrevious;
				if (result.pPrevious != NULL) (*result.pPrevious).pNext = result.pNext;
				if (result.pNext != NULL) (*result.pNext).pPrevious = result.pPrevious;
				free(qi);		// release queueItem memory
				// cleanup results
				result.pNext = NULL;
				result.pPrevious = NULL;
				(*globals).QueueCount -= 1;
			}
		}
		DSS_mutexUnlock((*globals).lock);

		return result;
	}

	// Pop item from the queue (protected using locks)
	// see queuePopUnlocked()
	queueItem queuePop(pglobalRecord globals, putilRecord utilid)
	{
		queueItem result;
		DSS_mutexLock((*globals).lock);
		result = queuePopUnlocked(globals, utilid);
		DSS_mutexUnlock((*globals).lock);
		return result;
	}

/*
** ===============================================================
** UDP socket management functions
** ===============================================================
*/
	// Changes the UDP port number in use
	// TODO: do we need this one?
	void setUDPPort (pglobalRecord globals, int newPort)
	{
		DSS_mutexLock((*globals).lock);
		if ((*globals).udpport != 0)
		{
			DSS_socketClose((*globals).socket); 
		}
		(*globals).udpport = newPort;
		if (newPort != 0)
		{
			(*globals).socket = DSS_socketNew(newPort);
		}
		DSS_mutexUnlock((*globals).lock);
	}

	// Gets the UDP port number in use
	// TODO: do we need this one?
	int getUDPPort (pglobalRecord globals)
	{
		int s;
		DSS_mutexLock((*globals).lock);
		s = (*globals).udpport;
		DSS_mutexUnlock((*globals).lock);
		return s;
	}

/*
** ===============================================================
** C API
** ===============================================================
*/
	// check utildid against list, 1 if it exists, 0 if not
	// Locking must be done by caller!
	int DSS_validutil(putilRecord utilid)
	{
		putilRecord id = UtilStart;	// start at top
		while ((id != NULL) && ((*id).pNext != utilid)) id = (*id).pNext;
		if (id == NULL) return 0;	// not found
		return 1; // found it
	}

	// Call this to deliver data to the queue
	// @returns; DSS_SUCCESS, DSS_ERR_UDP_SEND_FAILED, 
	// DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NOT_STARTED, DSS_ERR_INVALID_UTILID
	int DSS_deliver_1v0 (putilRecord utilid, DSS_decoder_1v0_t pDecode, void* pData)
	{
		pglobalRecord globals = (*utilid).pGlobals;	// TODO: should lock before accessing!! check others as well!
		int result = DSS_SUCCESS;	// report success by default
		int cnt;
		char buff[20];

		DSS_mutexLock((*globals).lock);	//TODO: shouldn't this be the utillock?
		if (DSS_validutil(utilid) == 0)
		{
			// invalid ID
			DSS_mutexUnlock((*globals).lock);
			return DSS_ERR_INVALID_UTILID;
		}
		if ((*globals).DSS_status != DSS_STATUS_STARTED)
		{
			// lib not started yet (or stopped already), exit
			DSS_mutexUnlock((*globals).lock);
			return DSS_ERR_NOT_STARTED;
		}

		cnt = queuePush(utilid, pDecode, pData);	// Push it on the queue
		if (cnt == DSS_ERR_OUT_OF_MEMORY) 
		{
			DSS_mutexUnlock((*globals).lock);
			return DSS_ERR_OUT_OF_MEMORY;
		}

		sprintf(buff, " %d", cnt);	// convert to string
		
		// Now send notification packet
		if ((*globals).udpport != 0)
		{
			if (DSS_socketSend((*globals).socket, buff) == 0)
			{
				// sending failed, retry; close create new and do again
				DSS_socketClose((*globals).socket);
				(*globals).socket = DSS_socketNew((*globals).udpport); 
				if (DSS_socketSend((*globals).socket, buff) == 0)
				{
					result = DSS_ERR_UDP_SEND_FAILED;		// report failure
				}
			}
		}

		DSS_mutexUnlock((*globals).lock);
		return result;	
	};

	// Returns the data associated with the Utility, or
	// NULL upon an invalid utilid
	void* DSS_getdata_1v0(putilRecord utilid)
	{
		void* result;
		DSS_mutexLock(utillock);	
		if (DSS_validutil(utilid))
		{
			result = (*utilid).pUtilData;
		}
		else
		{
			result = NULL;
		}
		DSS_mutexUnlock(utillock);
		return result;
	}

	// Sets the data associated with the Utility
	// Note; invalid utildid is ignored silently
	void DSS_setdata_1v0(putilRecord utilid, void* pData)
	{
		DSS_mutexLock(utillock);	
		if (DSS_validutil(utilid)) (*utilid).pUtilData = pData;
		DSS_mutexUnlock(utillock);
	}

	// register a library to use DSS 
	// @arg1; the globals record the utility is added to
	// @arg2; pointer to the cancel method of the utility, will be called
	// when DSS decides to terminate the collaboration with the utility
	// Returns: unique ID for the utility that must be used for all subsequent
	// calls to DSS, or NULL if it failed.
	// Failure reasons; DSS_ERR_NOT_STARTED, DSS_ERR_NO_CANCEL_PROVIDED or DSS_ERR_OUT_OF_MEMORY
	putilRecord DSS_register_1v0(lua_State *L, DSS_cancel_1v0_t pCancel, void* pData, int* errcode)
	{
		putilRecord util;
		putilRecord last;
		pglobalRecord globals = DSS_getvalidglobals(L); // will not return on failure 

		DSS_mutexLock(utillock);
		*errcode = DSS_SUCCESS;
		if ((*globals).DSS_status != DSS_STATUS_STARTED)
		{
			// DSS isn't running
			*errcode = DSS_ERR_NOT_STARTED;
			DSS_mutexUnlock(utillock);
			return NULL; 
		}

		if (pCancel == NULL)
		{
			*errcode = DSS_ERR_NO_CANCEL_PROVIDED;
			DSS_mutexUnlock(utillock);
			return NULL; 
		}

		// create and fill utility record
		util = malloc(sizeof(utilRecord));
		if (util == NULL) 
		{
			DSS_mutexUnlock(utillock);
			*errcode = DSS_ERR_OUT_OF_MEMORY;
			return NULL; //DSS_ERR_OUT_OF_MEMORY;
		}
		(*util).pCancel = pCancel;
		(*util).pGlobals = globals;
		(*util).pUtilData = pData;
		(*util).pNext = NULL;
		(*util).pPrevious = NULL;

		// Add record to end of list
		last = UtilStart;
		while (last != NULL || (*last).pNext != NULL) last = (*last).pNext;
		if (last == NULL)
		{
			// first item
			UtilStart = util;
		}
		else
		{
			// append to list
			(*last).pNext = util;
			(*util).pPrevious = last;
		}

		DSS_mutexUnlock(utillock);
		return util;
	}


	// unregisters a previously registered utility
	// cancels all items still in queue
	// returns DSS_SUCCESS, DSS_ERR_INVALID_UTILID
	int DSS_unregister_1v0(putilRecord utilid)
	{
		pglobalRecord globals;
		queueItem qi;

		DSS_mutexLock(utillock);
		if (DSS_validutil(utilid) == 0)
		{
			// invalid ID
			DSS_mutexUnlock(utillock);
			return DSS_ERR_INVALID_UTILID;
		}
		globals = (*utilid).pGlobals;

		// remove it from the list
		if (UtilStart == utilid) UtilStart = (*utilid).pNext;
		if ((*utilid).pNext != NULL) (*(*utilid).pNext).pPrevious = (*utilid).pPrevious;
		if ((*utilid).pPrevious != NULL) (*(*utilid).pPrevious).pNext = (*utilid).pNext;

		// cancel all items still in the queue
		qi = queuePop(globals, utilid);
		while (qi.pDecode != NULL)
		{
			qi.pDecode(NULL, qi.pData, utilid);	// no LuaState, use NULL to indicate cancelling
			qi = queuePop(globals, utilid);		// get next one
		}
		// free resources
		free(utilid);

		// Unlock, we're done with the util list
		DSS_mutexUnlock(utillock);
		return DSS_SUCCESS;
	}

/*
** ===============================================================
** Lua API
** ===============================================================
*/
	// Lua function to set the UDP port
	// @luaparam; UDP port number to use
	// @luareturns; 1 if successfull, or nil + error msg
	int L_setport(lua_State *L)
	{
		if (lua_gettop(L) >= 1 && luaL_checkint(L,1) >= 0 && luaL_checkint(L,1) <= 65535)
		{
			pglobalRecord globals = DSS_getvalidglobals(L); // won't return on error
			setUDPPort(globals, luaL_checkint(L,1));
			// report success
			lua_pushinteger(L, 1);
			return 1;
		}
		else
		{
			// There are no parameters, or the first isn't a number, call will not return
			return luaL_error(L, "Invalid UDP port number, use an integer value from 1 to 65535, or 0 to disable UDP notification");
		}
	};

	// Lua function to get the UDP port number in use
	// @luareturns: UDP portnumber 1-65535 in use, or 0 if no port
	int L_getport (lua_State *L)
	{
		pglobalRecord globals = DSS_getvalidglobals(L); // won't return on error
		DSS_mutexLock((*globals).lock);
		lua_pushinteger(L, (*globals).udpport);
		DSS_mutexUnlock((*globals).lock);
		return 1;
	};


	// Lua function to get the next item from the queue, its decode
	// function will be called to do what needs to be done
	// returns: queuesize of remaining items
	int L_poll(lua_State *L)
	{
		pglobalRecord globals = DSS_getvalidglobals(L); // won't return on error
		int cnt = 0;
		queueItem qi;

		// lockup and collect data and count at same time
		DSS_mutexLock((*globals).lock);
		qi = queuePopUnlocked(globals, DSS_LASTITEM);
		cnt = (*globals).QueueCount;
		DSS_mutexUnlock((*globals).lock);

		if (qi.pDecode != NULL)
		{
			// Call the decoder function with the data provided
			qi.pDecode(L, qi.pData, qi.utilid);
			qi.pData = NULL;
		}

		// push queue count as return value
		lua_pushinteger(L, cnt);	// return current queue size

		return 1;	// 1 return argument
	};

/*
** ===============================================================
** Library initialization
** ===============================================================
*/
	static const struct luaL_Reg DarkSideSync[] = {
		{"poll",L_poll},
		{"getport",L_getport},
		{"setport",L_setport},
		{NULL,NULL}
	};

DSS_API	int luaopen_darksidesync(lua_State *L)
	{
		pglobalRecord globals;

		if (DSS_initialized == NULL)  // TODO: first initialization of first mutex, this is unsafe! how to make it safe?
		{
			if (DSS_mutexInit(utillock) != 0)
			{
				// an error occured while initializing the 2 global mutexes
				return luaL_error(L,"DSS had an error initializing its mutexes");
			}
			DSS_mutexLock(utillock);
			if (DSS_mutexInit(statelock) != 0)
			{
				// an error occured while initializing the 2 global mutexes
				DSS_mutexUnlock(utillock);
				return luaL_error(L,"DSS had an error initializing its mutexes");
			}
			DSS_mutexUnlock(utillock);
		}

		// get or create the stateglobals
		globals = DSS_getstateglobals(L);
		(*globals).DSS_status = DSS_STATUS_STARTED;

		// Initializes API structure for API 1.0
		DSS_api_1v0.version = DSS_API_1v0_KEY;
		DSS_api_1v0.reg = &DSS_register_1v0;
		DSS_api_1v0.deliver = &DSS_deliver_1v0;
		DSS_api_1v0.getdata = &DSS_getdata_1v0;
		DSS_api_1v0.setdata = &DSS_setdata_1v0;
		DSS_api_1v0.unreg = &DSS_unregister_1v0;

		// Store pointer to my api structure in the Lua registry
		// for backgroundworkers to collect there
		lua_settop(L,0);						// clear stack
		lua_newtable(L);						// push a new table for DSS
		// add a DSS version key to the DSS table
		lua_pushstring(L, DSS_VERSION);
		lua_setfield(L, 1, DSS_VERSION_KEY);
		// add the DSS api version 1.0 to the DSS table
		lua_pushlightuserdata(L,&DSS_api_1v0);
		lua_setfield(L, 1, DSS_API_1v0_KEY);
		// Push overall DSS table onto the Lua registry
		lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

		// Increase state count and start network if first
		DSS_mutexLock(statelock);
		if (statecount == 0)
		{
			DSS_networkInit();
		}
		statecount = statecount + 1;
		DSS_mutexUnlock(statelock);

		luaL_register(L,"darksidesync",DarkSideSync);
		return 1;
	};


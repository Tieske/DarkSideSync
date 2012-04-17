#include <stdlib.h>
#include <lauxlib.h>
#include "udpsocket.h"
#include "locking.h"
#include "darksidesync.h"

static putilRecord volatile UtilStart = NULL;		// Holds first utility in the list
//static int volatile utilitycount = 0;				// Counter for registering utilities with unique IDs -- replaced by pointer to util structs

// TODO: add a status; running, stopping, stopped, including errors when attempting to register/deliver while stopping/stopped

/*
** ===============================================================
** Utility and globals registration functions
** ===============================================================
*/
	// Lookup a utility record for the given utilid
	// NOTE: will not lock the register, must be done before calling!!
	// Returns: utility record for the utilid, or NULL if not found
	putilRecord getUtility_________XXXXXXXXXXXXXXX(int utilid)
	{
		// TODO: remove no more need, the ID is the pointer to the util record
		putilRecord result = UtilStart;	//start at first item
		while (result != NULL && (*result).utilid != utilid)
		{
			result = (*result).pNext;
		}
		return result;
	}

	// Gets the pointer to the structure with all LuaState related globals
	// that are to be kept outside the LuaState (to be accessible from the
	// async callbacks)
	// If the structure is not found, a new one is created and initialized.
	pStateGlobals DSS_getstateglobals(lua_State *L)
	{
		//TODO: if it fails, handle errors from calling functions properly

		pglobalRecord globals;

		// try and collect the globals userdata
		lua_getfield(L, LUA_REGISTRYINDEX, DSS_GLOBALS_KEY)
		globals = lua_touserdata(L, -1)
		lua_pop(L,1)

		if (globals == NULL)
		{
			// Not found, create a new one
			lua_pop(L, 1)	// pop the failed result
			globals = lua_newuserdata(L, sizeof(pglobalRecord));
			lua_setfield(L, -1, DSS_GLOBALS_KEY);

			// now setup the empty structure content
			// TODO: setup empty global struct

			// now add a garbagecollect metamethod to clean it up afterwards
			// TODO: add garbage collect metamethod

		}

		return globals;
	}

	void DSS_clearstateglobals(lua_State *L)
	{
		// TODO: implement cleaning globals struct
	}

/*
** ===============================================================
** Queue management functions
** ===============================================================
*/
	// Push item in the queue
	// Returns number of items in queue, or DSS_ERR_OUT_OF_MEMORY if it failed
	int queuePush (putilRecord utilid, DSS_decoder_1v0_t pDecode, void* pData)
	{
		pqueueItem pqi = NULL;
		pglobalRecord globals = (*utilid).pGlobals;

		int cnt;

		if (NULL == (pqi = malloc(sizeof(queueItem))))
			return DSS_ERR_OUT_OF_MEMORY;	// exit, memory alloc failed

		(*pqi).utilid = utilid;
		(*pqi).pDecode = pDecode;
		(*pqi).pData = pData;
		(*pqi).pNext = NULL;
		(*pqi).pPrevious = NULL;

		lockQueue(globals);
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
		unlockQueue(globals);

		return cnt;
	}

	// Pop item from the queue
	// Returns queueItem filled, or all NULLs if none available
	// utilid is id of utility for which to return, or NULL to
	// just return the oldest item, independent of a utilid
	queueItem queuePop (pglobalRecord globals, putilRecord utilid)
	{
		pqueueItem qi;
		queueItem result;
		result.utilid = NULL;
		result.pData = NULL;
		result.pDecode = NULL;
		result.pNext = NULL;
		result.pPrevious = NULL;

		
		lockQueue(globals);
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
				QueueCount -= 1;
			}
		}
		unlockQueue(globals);

		return result;
	}

/*
** ===============================================================
** UDP socket management functions
** ===============================================================
*/
	// Changes the UDP port number in use
	void setUDPPort (pglobalRecord globals, int newPort)
	{
		lockSocket(globals);
		if ((*globals).DSS_UDPPort != 0)
		{
			destroySocket(globals); 
		}
		(*globals).DSS_UDPPort = newPort;
		if (newPort != 0)
		{
			createSocket(globals);
		}
		unlockSocket(globals);	
	}

	// Gets the UDP port number in use
	int getUDPPort (pglobalRecord globals)
	{
		int s;
		lockSocket(globals);
		s = (*globals).DSS_UDPPort;
		unlockSocket(globals);
		return s;
	}

/*
** ===============================================================
** C API
** ===============================================================
*/

	// Call this to deliver data to the queue
	// @returns; DSS_SUCCESS, DSS_ERR_INVALID_UTILID, DSS_ERR_UDP_SEND_FAILED, 
	// DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NOT_STARTED
	int DSS_deliver_1v0 (putilRecord utilid, DSS_decoder_1v0_t pDecode, void* pData)
	{
		pglobalRecord globals = (*utilid).pGlobals;
		int result = DSS_SUCCESS;	// report success by default
		int cnt;
		char buff[20];

		lockUtilList(globals);
		if ((*globals).DSS_status != DSS_STATUS_STARTED)
		{
			unlockUtilList(globals);
			return DSS_ERR_NOT_STARTED;
		}
		unlockUtilList(globals);
// TODO: if the piece below can be removed, also remove the DSS_ERR_INVALID_UTILID definition
/*		if (getUtility(utilid) == NULL)
		{
			return DSS_ERR_INVALID_UTILID;
		}
*/
		cnt = queuePush(utilid, pDecode, pData);	// Push it on the queue
		if (cnt == DSS_ERR_OUT_OF_MEMORY) return DSS_ERR_OUT_OF_MEMORY;

		sprintf(buff, " %d", cnt);	// convert to string
		
		// Now send notification packet
		lockSocket(globals);
		if ((*globals).DSS_UDPPort != 0)
		{
			if (sendPacket(globals, buff) == 0)
			{
				// sending failed, retry
				destroySocket(globals); 
				createSocket(globals);
				if (sendPacket(globals, buff) == 0)
					result = DSS_ERR_UDP_SEND_FAILED;		// report failure
			};
		}
		unlockSocket(globals);
		
		return result;	
	};

	// register a library to use DSS 
	// @arg1; the globals record the utility is added to
	// @arg2; pointer to the cancel method of the utility, will be called
	// when DSS decides to terminate the collaboration with the utility
	// Returns: unique ID for the utility that must be used for all subsequent
	// calls to DSS, or NULL if it failed.
	// Failure reasons; DSS_ERR_NOT_STARTED, DSS_ERR_NO_CANCEL_PROVIDED or DSS_ERR_OUT_OF_MEMORY
	putilRecord DSS_register_1v0(globals, DSS_cancel_1v0_t pCancel)
	{
		putilRecord util;
		putilRecord last;

		lockUtilList();
		if ((*globals).DSS_status != DSS_STATUS_STARTED)
		{
			unlockUtilList();
			return NULL; //DSS_ERR_NOT_STARTED;
		}
		unlockUtilList();

		if (pCancel == NULL)
		{
			return NULL; //DSS_ERR_NO_CANCEL_PROVIDED;
		}

		lockUtilList();
		//// find a unique ID that is > 0
		//while (utilitycount < 1 || getUtility(utilitycount) != NULL)
		//{
		//	utilitycount += 1;
		//	if (utilitycount < 1) utilitycount = 1;
		//}
		//newid = utilitycount;
		//utilitycount += 1;

		// create and fill utility record
		util = malloc(sizeof(utilRecord));
		if (util == NULL) 
		{
			unlockUtilList();
			return NULL; //DSS_ERR_OUT_OF_MEMORY;
		}
		(*util).pCancel = pCancel;
		(*util).utilid = newid;
		(*util).pGlobals = globals;
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

		unlockUtilList();
		return util;
	}


	// unregisters a previously registered utility
	// cancels all items still in queue
	void DSS_unregister_1v0(putilRecord utilid)
	{
		pglobalRecord globals = (*utilid).pGlobals
		queueItem qi;

		lockUtilList();
		// remove it from the list
		if (UtilStart == utilid) UtilStart = (*utilid).pNext;
		if ((*utilid).pNext != NULL) (*(*utilid).pNext).pPrevious = (*utilid).pPrevious;
		if ((*utilid).pPrevious != NULL) (*(*utilid).pPrevious).pNext = (*utilid).pNext;
		free(utilid);
		// Unlock, we're done with the util list
		unlockUtilList();

		// cancel all items still in the queue
		qi = queuePop(globals, utilid);
		while (qi.pDecode != NULL)
		{
			qi.pDecode(NULL, qi.pData);	// no LuaState, use NULL to indicate cancelling
			qi = queuePop(globals, utilid);		// get next one
		}
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
			pglobalRecord globals = DSS_getstateglobals(L);
			setUDPPort(globals, luaL_checkint(L,1));
			// report success
			lua_settop(L,0);
			lua_pushinteger(L, 1);
			return 1;
		}
		else
		{
			// There are no parameters, or the first isn't a number
			lua_settop(L,0);
			lua_pushnil(L);
			lua_pushstring(L, "Invalid UDP port number, use an integer value from 1 to 65535, or 0 to disable UDP notification");
			return 2;
		}
	};

	// Lua function to get the UDP port number in use
	// @luareturns: UDP portnumber 1-65535 in use, or 0 if no port
	int L_getport (lua_State *L)
	{
		pglobalRecord globals = DSS_getstateglobals(L);
		lockSocket(globals);
		lua_pushinteger(L, (*globals).DSS_UDPPort);
		unlockSocket(globals);
		return 1;
	};

	// Lua function to stop the library 
	// will cancel all registered utilities.
	int L_stop(lua_State *L)
	{
		pglobalRecord globals = DSS_getstateglobals(L);
		putilRecord listend = NULL;

		// Clear pointer to DSS api table in the Lua registry
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

		lockUtilList();
		(*globals).DSS_status = DSS_STATUS_STOPPING;

		// cancel all utilities, in reverse order
		while (UtilStart != NULL)
		{
			listend = UtilStart;
			while ((*listend).pNext != NULL) listend = (*listend).pNext;

			unlockUtilList();
			(*listend).pCancel(listend);	// call this utility cancel method
			lockUtilList();
		}
		
		(*globals).DSS_status = DSS_STATUS_STOPPED;
		unlockUtilList();
		return 0;
	}

	// Lua function to get the next item from the queue or
	// nil if none available
	int L_poll(lua_State *L)
	{
		pglobalRecord globals = DSS_getstateglobals(L);
		queueItem qi = queuePop(globals, DSS_LASTITEM);
		int cnt = 0;
		lua_settop(L,0);		// drop any argument provided

		if (qi.pDecode != NULL)
		{
			// Call the decoder function with the data provided
			qi.pDecode(L, qi.pData, qi.utilid);
			qi.pData = NULL;
			lua_settop(L,0);		// drop any arguments returned
			lockQueue(globals);
			lua_pushinteger(L, (*globals).QueueCount);	// return current queue size
			unlockQueue(globals);
		}
		else
		{
			// No data in queue, return nil
			lua_pushnil(L);
		}
		return 1;	// always 1 return argument
	};

/*
** ===============================================================
** Library initialization
** ===============================================================
*/
	static const struct luaL_Reg DarkSideSync[] = {
		{"stop",L_stop},
		{"poll",L_poll},
		{"getport",L_getport},
		{"setport",L_setport},
		{NULL,NULL}
	};

DSS_API	int luaopen_darksidesync(lua_State *L){
//TODO: add a garbage collector that stops the queue and clients, add it to metamethods of globals struct
//TODO: reorder this function, some early accessed variables are inside the stateglobal structure created later on

		// get or create the stateglobals
		pglobalRecord globals = DSS_getstateglobals(L);

		if (initLocks() != 0)
		{
			// Mutexes could not be created
			luaL_error(L, "Mutexes could not be created"); // call never returns
		}

		lockSocket();
		DSS_UDPPort = 0;
		unlockSocket();

		// Initializes API structure for API 1.0
		DSS_api_1v0.version = DSS_API_1v0_KEY;
		DSS_api_1v0.reg = &DSS_register_1v0;
		DSS_api_1v0.deliver = &DSS_deliver_1v0;
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

		lockUtilList();
		DSS_status = DSS_STATUS_STARTED;
		unlockUtilList();

		luaL_register(L,"darksidesync",DarkSideSync);
		return 1;
	};


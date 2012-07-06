#include <stdlib.h>
#include <lauxlib.h>
#include "udpsocket.h"
#include "locking.h"
#include "darksidesync.h"

static putilRecord volatile UtilStart = NULL;		// Holds first utility in the list
static void* volatile DSS_initialized = NULL;		// while its NULL, the first mutex is uninitialized
static DSS_mutex_t utillock;						// lock to list of utilities
static int statecount = 0;							// counter for number of lua states using this lib
static DSS_mutex_t statelock;						// lock to protect the state counter
static DSS_api_1v0_t DSS_api_1v0;					// API struct for version 1.0

// forward definitions
void setUDPPort (pglobalRecord g, int newPort);

#ifdef _DEBUG
//can be found here  http://www.lua.org/pil/24.2.3.html
static void stackDump (lua_State *L, const char *text) {
      int i;
      int top = lua_gettop(L);
	  if (text == NULL)
		printf("--------Start Dump------------\n");
	  else
	    printf("--------Start %s------------\n", text);
      for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
    
          case LUA_TSTRING:  /* strings */
            printf("`%s'", lua_tostring(L, i));
            break;
    
          case LUA_TBOOLEAN:  /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
    
          case LUA_TNUMBER:  /* numbers */
            printf("%g", lua_tonumber(L, i));
            break;
    
          default:  /* other values */
            printf("%s", lua_typename(L, t));
            break;
    
        }
        printf("  ");  /* put a separator */
      }
      printf("\n");  /* end the listing */
	  printf("--------End Dump------------\n");
    }
#endif

/*
** ===============================================================
** Utility and globals registration functions
** ===============================================================
*/
// Returns 1 if the LuaState has a global struct, 0 otherwise
static int DSS_hasstateglobals(lua_State *L)
{
	pglobalRecord g;
	// try and collect the globals userdata
	lua_getfield(L, LUA_REGISTRYINDEX, DSS_GLOBALS_KEY);
	g = (pglobalRecord)lua_touserdata(L, -1);
	lua_pop(L,1);
	if (g == NULL) return 0;
	return 1;
}

// Creates a new global record and stores it in the registry
// will overwrite existing if present!!
// Returns: created globalrecord or NULL on failure
// provide errno may be NULL; possible errcode;
// DSS_SUCCESS, DSS_ERR_OUT_OF_MEMORY, DSS_ERR_INIT_MUTEX_FAILED, DSS_ERR_NO_GLOBALS
static pglobalRecord DSS_newstateglobals(lua_State *L, int* errcode)
{
	pglobalRecord g;

	int le;	// local errorcode
	if (errcode == NULL) errcode = &le;
	*errcode = DSS_SUCCESS;

	// create a new one
	g = (pglobalRecord)lua_newuserdata(L, sizeof(globalRecord));
	if (g == NULL) *errcode = DSS_ERR_OUT_OF_MEMORY;	// alloc failed

	if (*errcode == DSS_SUCCESS) 
	{
		// now setup UDP port and status
		g->udpport = 0;
		g->socket = DSS_socketNew(g->udpport);
		g->DSS_status = DSS_STATUS_STOPPED;

		// setup data queue
		if (DSS_mutexInitx(&(g->lock)) != 0) *errcode = DSS_ERR_INIT_MUTEX_FAILED;
	}

	if (*errcode == DSS_SUCCESS)
	{
		g->QueueCount = 0;
		g->QueueEnd = NULL;
		g->QueueStart = NULL;
		g->UserdataStart = NULL;
	}

	if (*errcode != DSS_SUCCESS)	// we had an error
	{
		// report error, errcode contains details
		return NULL;
	}

	// now add a garbagecollect metamethod and anchor it
	luaL_getmetatable(L, DSS_GLOBALS_MT);	// get metatable with GC method
	lua_setmetatable(L, -2);				// set it to the created userdata
	lua_setfield(L, LUA_REGISTRYINDEX, DSS_GLOBALS_KEY);	// anchor the userdata

	return g;
}

// Gets the pointer to the structure with all LuaState related g
// that are to be kept outside the LuaState (to be accessible from the
// async callbacks)
// Locking isn't necessary, as the structure is collected from/added to
// the Lua_State, hence the calling code must already be single threaded.
// returns NULL upon failure, use errcode for details.
// provided errno may be NULL; possible errcode;
// DSS_SUCCESS, DSS_ERR_NO_GLOBALS
// see also DSS_getvalidglobals()
static pglobalRecord DSS_getstateglobals(lua_State *L, int* errcode)
{

	pglobalRecord g;

	int le;	// local errorcode
	if (errcode == NULL) errcode = &le;
	*errcode = DSS_SUCCESS;

	// try and collect the globals userdata
	lua_getfield(L, LUA_REGISTRYINDEX, DSS_GLOBALS_KEY);
	g = (pglobalRecord)lua_touserdata(L, -1);
	lua_pop(L,1);

	if (g == NULL) *errcode = DSS_ERR_NO_GLOBALS;

	return g;
}

// checks global struct, returns 1 if valid, 0 otherwise
// Caller should lock global record!!
static int DSS_isvalidglobals(pglobalRecord g)
{
	int result = 1; // assume valid
	if (g == NULL)			// no use, caller should have locked, hence have checked NULL ....
	{
		result = 0;	// not valid
	}
	else 
	{
		if  (g->DSS_status != DSS_STATUS_STARTED)	result = 0;	// not valid
	}
	return result;
}

// returns a valid globals struct or fails with a Lua error (and won't
// return in that case)
static pglobalRecord DSS_getvalidglobals(lua_State *L)
{
	pglobalRecord g = DSS_getstateglobals(L, NULL); // no errorcode required
	if (g == NULL) 
	{
		// following error call will not return
		luaL_error(L, "DSS was not started yet, or already stopped");
	}
	DSS_mutexLockx(&(g->lock));	
	if (DSS_isvalidglobals(g) == 0) 
	{
		DSS_mutexUnlockx(&(g->lock));	
		// following error call will not return
		luaL_error(L, "DSS was not started yet, or already stopped");
	}
	DSS_mutexUnlockx(&(g->lock));	
	return g;
}

// Garbage collect function for the global userdata
// DSS is exiting from this LuaState, so clean it all up
static int DSS_clearstateglobals(lua_State *L)
{
	pglobalRecord g;
	putilRecord listend;

	g = (pglobalRecord)lua_touserdata(L, 1);		// first param is userdata to destroy

#ifdef _DEBUG
	OutputDebugStringA("DSS: Unloading DSS ...\n");
#endif
	// Set status to stopping, registering and delivering will fail from here on
	DSS_mutexLockx(&(g->lock));
	g->DSS_status = DSS_STATUS_STOPPING;
	DSS_mutexUnlockx(&(g->lock));	// unlock to let any waiting threads move in (and fail, because we're stopping)
	

	// cancel all utilities, in reverse order
	DSS_mutexLockx(&utillock);
	while (UtilStart != NULL)
	{
		listend = UtilStart;
		while (listend->pNext != NULL) listend = listend->pNext;

		DSS_mutexUnlockx(&utillock);		// must unlock to let the cancel function succeed
		listend->pCancel(listend, listend->pUtilData);	// call this utility's cancel method
		DSS_mutexLockx(&utillock);		// lock again to get the next one
	}
	DSS_mutexUnlockx(&utillock);
	
	// update status again, we're done stopping
	DSS_mutexLockx(&(g->lock));
	g->DSS_status = DSS_STATUS_STOPPED;
	DSS_mutexUnlockx(&(g->lock));	

	// Remove references from the registry
	//lua_pushnil(L);
	//lua_setfield(L, -1, DSS_GLOBALS_KEY);
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

	// Close socket and destroy mutex
	setUDPPort(g, 0);  // set port to 0, will close socket
	DSS_mutexDestroyx(&(g->lock));

	// Reduce state count and close network if none left
	DSS_mutexLockx(&statelock);
	statecount = statecount - 1;
	if (statecount == 0)
	{
		DSS_networkStop();
	}
	DSS_mutexUnlockx(&statelock);
#ifdef _DEBUG
	OutputDebugStringA("DSS: Unloading DSS completed\n");
#endif
	return 0;
}

/*
** ===============================================================
** Queue management functions
** ===============================================================
*/
// Push item in the queue (will NOT lock utils nor globals)
// Returns waithandle or NULL,
// resultitem; will contain the queueItem as created
// result; will contain number of items in queue, or DSS_ERR_OUT_OF_MEMORY or 
// DSS_ERR_NOTSTARTED if it failed
static pDSS_waithandle queuePush (putilRecord utilid, DSS_decoder_1v0_t pDecode, DSS_return_1v0_t pReturn, void* pData, pqueueItem* resultitem, int* result)
{
	int cnt;
	pDSS_waithandle wh = NULL;
	pqueueItem pqi = NULL;
	pglobalRecord g; 
	
	g = utilid->pGlobals;		// grab globals

	if (DSS_isvalidglobals(g) == 0) 
	{
		*result = DSS_ERR_NOT_STARTED;
		return NULL;
	}

	if (NULL == (pqi = (pqueueItem)malloc(sizeof(queueItem))))
	{
		*result = DSS_ERR_OUT_OF_MEMORY;
		return NULL;	// exit, memory alloc failed
	}

	if (pReturn != NULL)
	{
		wh = DSS_waithandle_create();
		if (wh == NULL)
		{
			// error, resource alloc failed
			free(pqi);
			*result = DSS_ERR_OUT_OF_MEMORY; 
			return NULL; 
		}
	}

	pqi->pWaitHandle = wh;
	pqi->utilid = utilid;
	pqi->pDecode = pDecode;
	pqi->pReturn = pReturn;
	pqi->pData = pData;
	pqi->pNext = NULL;
	pqi->pPrevious = NULL;

	if (g->QueueStart == NULL)
	{
		// first item in queue
		g->QueueStart = pqi;
		g->QueueEnd = pqi;
	}
	else
	{
		// append to queue
		g->QueueEnd->pNext = pqi;
		pqi->pPrevious = g->QueueEnd;
		g->QueueEnd = pqi;
	}
	g->QueueCount += 1;
	cnt = g->QueueCount;

	if (resultitem != NULL)  (*resultitem) = pqi;  // set result
	*result = cnt;
	return wh;
}

// Pop item from the queue, without locking; caller must lock globals!
// Returns queueItem filled, or all NULLs if none available
// utilid is id of utility for which to return, or NULL to
// just return the oldest item, independent of a utilid
static queueItem queuePopUnlocked(pglobalRecord g, putilRecord utilid)
{
	pqueueItem qi;
	queueItem result;
	result.utilid = NULL;
	result.pData = NULL;
	result.pDecode = NULL;
	result.pWaitHandle = NULL;
	result.pReturn = NULL;
	result.pNext = NULL;
	result.pPrevious = NULL;

	if (g->QueueStart != NULL)
	{

		if (utilid == NULL)
		{
			// just grab the oldest from the queue
			qi = g->QueueStart;
		}
		else
		{
			// traverse backwards to find the last one with a corresponding utilid
			qi = g->QueueStart;
			while (qi != NULL && qi->utilid != utilid)
			{
				qi = qi->pPrevious;
			}
		}

		if (qi != NULL) 
		{
			result = *qi;
			// dismiss item from linked list
			if (qi == g->QueueStart) g->QueueStart = qi->pNext;
			if (qi == g->QueueEnd) g->QueueEnd = qi->pPrevious;
			if (result.pPrevious != NULL) result.pPrevious->pNext = result.pNext;
			if (result.pNext != NULL) result.pNext->pPrevious = result.pPrevious;
			free(qi);		// release queueItem memory
			// cleanup results
			result.pNext = NULL;
			result.pPrevious = NULL;
			g->QueueCount -= 1;
		}
	}

	return result;
}

// Pop item from the queue (protected using locks)
// see queuePopUnlocked()
static queueItem queuePop(pglobalRecord g, putilRecord utilid)
{
	queueItem result;
	DSS_mutexLockx(&(g->lock));
	result = queuePopUnlocked(g, utilid);
	DSS_mutexUnlockx(&(g->lock));
	return result;
}

/*
** ===============================================================
** UDP socket management functions
** ===============================================================
*/
// Changes the UDP port number in use
// uses the lock on the globals, will only be called from Lua
static void setUDPPort (pglobalRecord g, int newPort)
{
	DSS_mutexLockx(&(g->lock));
	if (g->udpport != 0)
	{
		DSS_socketClose(g->socket); 
	}
	g->udpport = newPort;
	if (newPort != 0)
	{
		g->socket = DSS_socketNew(newPort);
	}
	DSS_mutexUnlockx(&(g->lock));
}

/*
** ===============================================================
** C API
** ===============================================================
*/
// check utildid against list, 1 if it exists, 0 if not
// Locking utillock must be done by caller!
static int DSS_validutil(putilRecord utilid)
{
	putilRecord id = UtilStart;	// start at top
	if (utilid != id)
	{
		while ((id != NULL) && (id->pNext != utilid)) id = id->pNext;
		if (id == NULL) return 0;	// not found
	}
	return 1; // found it
}

// Call this to deliver data to the queue
// @returns; DSS_SUCCESS, DSS_ERR_UDP_SEND_FAILED, 
// DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NOT_STARTED, DSS_ERR_INVALID_UTILID
static int DSS_deliver_1v0 (putilRecord utilid, DSS_decoder_1v0_t pDecode, DSS_return_1v0_t pReturn, void* pData)
{
	pglobalRecord g;
	int result = DSS_SUCCESS;	// report success by default
	int cnt;
	char buff[20];
	pqueueItem pqi;
	pDSS_waithandle wh;

#ifdef _DEBUG
	OutputDebugStringA("DSS: Start delivering data ...\n");
#endif

	DSS_mutexLockx(&utillock);
	if (DSS_validutil(utilid) == 0)
	{
		// invalid ID
		DSS_mutexUnlockx(&utillock);
		return DSS_ERR_INVALID_UTILID;
	}

	g = utilid->pGlobals;	
	DSS_mutexLockx(&(g->lock));
	DSS_mutexUnlockx(&utillock);
	if (g->DSS_status != DSS_STATUS_STARTED)
	{
		// lib not started yet (or stopped already), exit
		DSS_mutexUnlockx(&(g->lock));
		return DSS_ERR_NOT_STARTED;
	}

	wh = queuePush(utilid, pDecode, pReturn, pData, &pqi, &cnt);	// Push it on the queue
	if (cnt == DSS_ERR_OUT_OF_MEMORY) 
	{
		DSS_mutexUnlockx(&(g->lock));
		return DSS_ERR_OUT_OF_MEMORY;
	}

	sprintf(buff, " %d", cnt);	// convert to string
	
	// Now send notification packet
	if (g->udpport != 0)
	{
		if (DSS_socketSend(g->socket, buff) == 0)
		{
			// sending failed, retry; close create new and do again
			DSS_socketClose(g->socket);
			g->socket = DSS_socketNew(g->udpport); 
			if (DSS_socketSend(g->socket, buff) == 0)
			{
				result = DSS_ERR_UDP_SEND_FAILED;	// store failure to report
			}
		}
	}

	DSS_mutexUnlockx(&(g->lock));
	if (wh != NULL)
	{
		// A waithandle was created, so we must go and wait for the queued item to be completed
		DSS_waithandle_wait(wh);	// blocks until released
		DSS_waithandle_delete(wh);	// destroy waithandle
	}

#ifdef _DEBUG
	OutputDebugStringA("DSS: End delivering data ...\n");
#endif
	return result;	
};

// Returns the data associated with the Utility, or
// NULL upon an invalid utilid
static void* DSS_getdata_1v0(putilRecord utilid, int* errcode)
{
	void* result;
	
	int le;	// local errorcode
	if (errcode == NULL) errcode = &le;
	*errcode = DSS_SUCCESS;

	DSS_mutexLockx(&utillock);	
	if (DSS_validutil(utilid))
	{
		result = utilid->pUtilData;
	}
	else
	{
		*errcode = DSS_ERR_INVALID_UTILID;
		result = NULL;
	}
	DSS_mutexUnlockx(&utillock);
	return result;
}

// Sets the data associated with the Utility
// Return DSS_SUCCESS or DSS_ERR_INVALID_UTILID
static int DSS_setdata_1v0(putilRecord utilid, void* pData)
{
	int result = DSS_SUCCESS;
	DSS_mutexLockx(&utillock);	
	if (DSS_validutil(utilid))
	{
		utilid->pUtilData = pData;
	}
	else
	{
		result = DSS_ERR_INVALID_UTILID;
	}
	DSS_mutexUnlockx(&utillock);
	return result;
}

// Gets the utilid based on a LuaState and libid
// return NULL upon failure, see Errcode for details; DSS_SUCCESS,
// DSS_ERR_NOT_STARTED or DSS_ERR_UNKNOWN_LIB
static void* DSS_getutilid_1v0(lua_State *L, void* libid, int* errcode)
{
	pglobalRecord g = DSS_getstateglobals(L, NULL);
	putilRecord utilid = NULL;

	int le;	// local errorcode
	if (errcode == NULL) errcode = &le;
	*errcode = DSS_SUCCESS;

	if (g != NULL)
	{
		DSS_mutexLockx(&(g->lock));
		if (g->DSS_status != DSS_STATUS_STARTED)
		{
			DSS_mutexUnlockx(&(g->lock));
			*errcode = DSS_ERR_NOT_STARTED;
			return NULL;
		}
		DSS_mutexUnlockx(&(g->lock));

		// we've got a set of globals, now compare this to the utillist
		DSS_mutexLockx(&utillock);
		utilid = UtilStart;
		while (utilid != NULL)
		{
			if ((utilid->pGlobals == g) && (utilid->libid == libid))
			{
				//This utilid matches both the LuaState (globals) and the libid.
				DSS_mutexUnlockx(&utillock);
				return utilid;	// found it, return and exit.
			}

			// No match, try next one in the list
			utilid = utilid->pNext;
		}
		DSS_mutexUnlockx(&utillock);
	}
	else
	{
		// No Globals found
		*errcode = DSS_ERR_NOT_STARTED;
		return NULL;
	}
	// we had globals, failed anyway, so no valid utilid
	*errcode = DSS_ERR_UNKNOWN_LIB;
	return NULL;	// failed
}

// register a library to use DSS 
// @arg1; the globals record the utility is added to
// @arg2; pointer to the cancel method of the utility, will be called
// when DSS decides to terminate the collaboration with the utility
// Returns: unique ID for the utility that must be used for all subsequent
// calls to DSS, or NULL if it failed.
// Failure reasons; DSS_ERR_NOT_STARTED, DSS_ERR_NO_CANCEL_PROVIDED, 
//                  DSS_ERR_ALREADY_REGISTERED, DSS_ERR_OUT_OF_MEMORY
static putilRecord DSS_register_1v0(lua_State *L, void* libid, DSS_cancel_1v0_t pCancel, void* pData, int* errcode)
{
	putilRecord util;
	putilRecord last;
	pglobalRecord g; 

	int le;	// local errorcode
	if (errcode == NULL) errcode = &le;
	*errcode = DSS_SUCCESS;

#ifdef _DEBUG
	OutputDebugStringA("DSS: Start registering lib ...\n");
#endif
	if (pCancel == NULL)
	{
		*errcode = DSS_ERR_NO_CANCEL_PROVIDED;
		return NULL; 
	}

	g = DSS_getstateglobals(L, NULL); 
	if (g == NULL)
	{
		*errcode = DSS_ERR_NOT_STARTED;
		return NULL;
	}

	if (DSS_getutilid_1v0(L, libid, NULL) != NULL)
	{
		// We got an ID returned, so this lib is already registered
		*errcode = DSS_ERR_ALREADY_REGISTERED;
		return NULL;
	}

	DSS_mutexLockx(&utillock);
	DSS_mutexLockx(&(g->lock));
	if (g->DSS_status != DSS_STATUS_STARTED)
	{
		// DSS isn't running
		*errcode = DSS_ERR_NOT_STARTED;
		DSS_mutexUnlockx(&(g->lock));
		DSS_mutexUnlockx(&utillock);
		return NULL; 
	}

	// create and fill utility record
	util = (putilRecord)malloc(sizeof(utilRecord));
	if (util == NULL) 
	{
		DSS_mutexUnlockx(&(g->lock));
		DSS_mutexUnlockx(&utillock);
		*errcode = DSS_ERR_OUT_OF_MEMORY;
		return NULL; //DSS_ERR_OUT_OF_MEMORY;
	}
	util->pCancel = pCancel;
	util->pGlobals = g;
	util->pUtilData = pData;
	util->libid = libid;
	util->pNext = NULL;
	util->pPrevious = NULL;

	// Add record to end of list
	last = UtilStart;
	while (last != NULL && last->pNext != NULL) last = last->pNext;
	if (last == NULL)
	{
		// first item
		UtilStart = util;
	}
	else
	{
		// append to list
		last->pNext = util;
		util->pPrevious = last;
	}

	DSS_mutexUnlockx(&(g->lock));
	DSS_mutexUnlockx(&utillock);
#ifdef _DEBUG
	OutputDebugStringA("DSS: Done registering lib ...\n");
#endif
	return util;
}


// unregisters a previously registered utility
// cancels all items still in queue
// returns DSS_SUCCESS, DSS_ERR_INVALID_UTILID
static int DSS_unregister_1v0(putilRecord utilid)
{
	pglobalRecord g;
	queueItem qi;
	pqueueItem pqi = NULL;

#ifdef _DEBUG
	OutputDebugStringA("DSS: Start unregistering lib ...\n");
#endif
	DSS_mutexLockx(&utillock);
	if (DSS_validutil(utilid) == 0)
	{
		// invalid ID
		DSS_mutexUnlockx(&utillock);
		return DSS_ERR_INVALID_UTILID;
	}
	g = utilid->pGlobals;

	// remove it from the list
	if (UtilStart == utilid) UtilStart = utilid->pNext;
	if (utilid->pNext != NULL) utilid->pNext->pPrevious = utilid->pPrevious;
	if (utilid->pPrevious != NULL) utilid->pPrevious->pNext = utilid->pNext;

	// cancel all items still in the queue
	qi = queuePop(g, utilid);
	while (qi.pDecode != NULL)
	{
		qi.pDecode(NULL, qi.pData, utilid);	// no LuaState, use NULL to indicate cancelling
		if (qi.pWaitHandle != NULL)
		{
			// release and delete waithandle
			DSS_waithandle_signal(qi.pWaitHandle);
			//DSS_waithandle_delete(qi.pWaitHandle);	will be destroyed by the waiting thread
		}
		qi = queuePop(g, utilid);		// get next one
	}

	// Cancel all items stored in userdatas
	pqi = g->UserdataStart;
	while (pqi != NULL)
	{
		if (pqi->utilid == utilid)
		{
			// need to cancel this one, as it has our ID
			// first remove from list
			if (pqi->pPrevious != NULL)
			{
				pqi->pPrevious->pNext = pqi->pNext;
				if (pqi->pNext != NULL) 
					pqi->pNext->pPrevious = pqi->pPrevious;
				else
					pqi->pNext = NULL;
			}
			else
			{
				g->UserdataStart = pqi->pNext;
				if (pqi->pNext != NULL) pqi->pNext->pPrevious = NULL;
			}

			// cancel the item
			if (pqi->pReturn != NULL)
			{
				// Call return function with lua_State == NULL, to indicate cancelling
				pqi->pReturn(NULL, pqi->pData, pqi->utilid, FALSE);
				pqi->pReturn = NULL;	// set to done, so GC can collect without calling again
			}
		}

		// move to next item in list
		pqi = pqi->pNext;
	}

	// free resources
	free(utilid);

	// Unlock, we're done with the util list
	DSS_mutexUnlockx(&utillock);
#ifdef _DEBUG
	OutputDebugStringA("DSS: Done unregistering lib ...\n");
#endif
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
static int L_setport(lua_State *L)
{
	if (lua_gettop(L) >= 1 && luaL_checkint(L,1) >= 0 && luaL_checkint(L,1) <= 65535)
	{
		pglobalRecord g = DSS_getvalidglobals(L); // won't return on error
		setUDPPort(g, luaL_checkint(L,1));
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
static int L_getport (lua_State *L)
{
	pglobalRecord g = DSS_getvalidglobals(L); // won't return on error
	DSS_mutexLockx(&(g->lock));
	lua_pushinteger(L, g->udpport);
	DSS_mutexUnlockx(&(g->lock));
	return 1;
};


// Lua function to get the next item from the queue, its decode
// function will be called to do what needs to be done
// returns: queuesize of remaining items, followed by any stuff left by decoder
// or -1 if there was nothing on the queue to begin with.
static int L_poll(lua_State *L)
{
	pglobalRecord g = DSS_getvalidglobals(L); // won't return on error
	int cnt = 0;
	int res = 0;
	int err = DSS_SUCCESS;
	queueItem qi;
	pqueueItem qi2;

	lua_settop(L, 0);		// clear stack

	// lock and collect data and count at same time
	DSS_mutexLockx(&utillock);
	DSS_mutexLockx(&(g->lock));
	qi = queuePopUnlocked(g, DSS_LASTITEM);
	cnt = g->QueueCount;
	DSS_mutexUnlockx(&(g->lock));
	if (qi.pDecode != NULL)
	{
		// keep utillock while calling this to prevent unregistering util meanwhile
		// In this construct utilid will be valid!
		// execute callback
		res = qi.pDecode(L, qi.pData, qi.utilid);	
	}

	if (qi.pDecode != NULL)
	{
		qi.pDecode = NULL;				// set to NULL to indicate call is done
		if (res < 1)	// indicator transaction is complete, do NOT create the userdata and do not call return callback
		{
			qi.pReturn = NULL;
			if (qi.pWaitHandle != NULL)
			{
				DSS_waithandle_signal(qi.pWaitHandle);
				//DSS_waithandle_delete(qi.pWaitHandle);	will be destroyed by the waiting thread
				qi.pWaitHandle = NULL;
			}
			lua_pushinteger(L, cnt);	// add count to results
			DSS_mutexUnlockx(&utillock);
			return 1;					// Only count is returned
		}
		lua_checkstack(L, 2);
		if (qi.pReturn != NULL)
		{
			// Create userdata to store the queueitem
			qi2 = (pqueueItem)lua_newuserdata(L, sizeof(queueItem));
			if (qi2 == NULL)
			{
				// memory allocation error, exit process here
				qi.pReturn(NULL, qi.pData, qi.utilid, FALSE); // call with lua_State == NULL to have it cancelled
				DSS_waithandle_signal(qi.pWaitHandle);
				//DSS_waithandle_delete(qi.pWaitHandle);	will be destroyed by the waiting thread
				lua_pushinteger(L, cnt);	// add count to results
				DSS_mutexUnlockx(&utillock);
				return 1;					// Only count is returned
			}
			*qi2 = qi;	// copy contents to the userdata

			// attach metatable
			luaL_getmetatable(L, DSS_QUEUEITEM_MT);
			lua_setmetatable(L, -2);

			// store in userdata list
			g = DSS_getstateglobals(L, &err);
			if (err != DSS_SUCCESS || g == NULL)
			{
				DSS_mutexUnlockx(&utillock);
				return luaL_error(L, "DSS: Internal error, global record is not available while QueueItems are still around (move to userdata)!!");
			}
			qi2->pNext = g->UserdataStart;
			qi2->pPrevious = NULL;
			if (qi2->pNext != NULL) qi2->pNext->pPrevious = qi2;

			// Move userdata (on top) to 2nd position, directly after the lua callback function
			if (lua_gettop(L) > 2 ) lua_insert(L, 2);
			res = res + 1;		// 1 more result because we added the userdata
		}
		lua_pushinteger(L, cnt);	// add count to results
		lua_insert(L,1);			// move count to 1st position
		DSS_mutexUnlockx(&utillock);
		return (res + 1);			// 1 more result because of the count that was added
	}
	else
	{
		// push queue count as return value
		DSS_mutexUnlockx(&utillock);
		lua_pushinteger(L, -1);	// return -1 to indicate queue was empty when called
		return 1;	// 1 return argument
	}
};

// Execute the return callback, either regular or from garbage collector
static int L_return_internal(lua_State *L, BOOL garbage)
{
	int res = 0;
	int err = DSS_SUCCESS;
	pglobalRecord g = NULL;
	pqueueItem qi = (pqueueItem)luaL_checkudata(L, 1, DSS_QUEUEITEM_MT);	// first item must be our queue item
	lua_remove(L, 1);	// remove userdata from stack

	DSS_mutexLockx(&utillock);
	if (qi->pReturn == NULL)
	{
		// No more return function so the queueitem has been cancelled, or been dealt with already, so it must be garbage
		if (! garbage)
		{
			// its not garbage, so something went wrong
			// 'return' method was called on an invalid utilityitem
			DSS_mutexUnlockx(&utillock);
			if (DSS_validutil(qi->utilid))
			{
				return luaL_error(L, "DSS: Error calling 'return' method. Method was called more than once or the item was cancelled by the originating library.");
			}
			else
			{
				return luaL_error(L, "DSS: Error calling 'return' method, originating library has unregistered from DSS and is no longer available");
			}
		}
		// So it is garbage, as it is supposed to be here, nothing to do, just unlock and return
		DSS_mutexUnlockx(&utillock);
		return 0;
	}

	// keep utillock while calling this to prevent unregistering util meanwhile
	// In this construct utilid will be valid!
	// Move it off-list
	if (qi->pPrevious == NULL)
	{
		// its the first item in the list, so we need the global record to update the pointer there
		g = DSS_getstateglobals(L, &err);
		if (err != DSS_SUCCESS || g == NULL)
		{
			DSS_mutexUnlockx(&utillock);
			return luaL_error(L, "DSS: Internal error, global record is not available while QueueItems are still around (finish userdata)!!");
		}
		g->UserdataStart = qi->pNext;
		if (qi->pNext != NULL) qi->pNext->pPrevious = NULL;
		g = NULL;
	}
	else
	{
		// its somewhere mid-list, just update
		qi->pPrevious->pNext = qi->pNext;
		if (qi->pNext != NULL)	qi->pNext->pPrevious = qi->pPrevious;
	}
	qi->pNext = NULL;
	qi->pPrevious = NULL;

	// now execute callback, here the utility should release all resources
	res = qi->pReturn(L, qi->pData, qi->utilid, garbage);	
	qi->pReturn = NULL;
	qi->pData = NULL;
	if (qi->pWaitHandle != NULL)
	{
		DSS_waithandle_signal(qi->pWaitHandle);
		//DSS_waithandle_delete(qi->pWaitHandle);	will be destroyed by the waiting thread
		qi->pWaitHandle = NULL;
	}

	DSS_mutexUnlockx(&utillock);
	return res;
}

// GC method for queue items waiting for a 'return' callback
static int L_queueItemGC(lua_State *L)
{
	return L_return_internal(L, TRUE);
}

// Return method for queue items waiting for a 'return' callback
static int L_return(lua_State *L)
{
	return L_return_internal(L, FALSE);
}

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
	pglobalRecord g;
	int errcode;

#ifdef _DEBUG
OutputDebugStringA("DSS: LuaOpen started...\n");
#endif

	if (DSS_initialized == NULL)  // TODO: first initialization of first mutex, this is unsafe! how to make it safe?
	{
		DSS_initialized = &DSS_initialized;	// point to itself, no longer NULL
		if (DSS_mutexInitx(&utillock) != 0)
		{
			// an error occured while initializing the 2 global mutexes
			return luaL_error(L,"DSS had an error initializing its mutexes (utillock)");
		}
		DSS_mutexLockx(&utillock);
		if (DSS_mutexInitx(&statelock) != 0)
		{
			// an error occured while initializing the 2 global mutexes
			DSS_mutexUnlockx(&utillock);
			return luaL_error(L,"DSS had an error initializing its mutexes (statelock)");
		}
		DSS_mutexUnlockx(&utillock);
	}

	// Initializes API structure for API 1.0
	DSS_api_1v0.version = DSS_API_1v0_KEY;
	DSS_api_1v0.reg = (DSS_register_1v0_t)&DSS_register_1v0;
	DSS_api_1v0.getutilid = (DSS_getutilid_1v0_t)&DSS_getutilid_1v0;
	DSS_api_1v0.deliver = (DSS_deliver_1v0_t)&DSS_deliver_1v0;
	DSS_api_1v0.getdata = (DSS_getdata_1v0_t)&DSS_getdata_1v0;
	DSS_api_1v0.setdata = (DSS_setdata_1v0_t)&DSS_setdata_1v0;
	DSS_api_1v0.unreg = (DSS_unregister_1v0_t)&DSS_unregister_1v0;

	// Create metatable for userdata's waiting for 'return' callback
	luaL_newmetatable(L, DSS_QUEUEITEM_MT);
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);  
	lua_settable(L, -3);	// copy metatable itself
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, &L_queueItemGC);
	lua_settable(L, -3);
	lua_pushstring(L, "return");
	lua_pushcfunction(L, &L_return);
	lua_settable(L, -3);

	// Create a metatable to GC the global data upon exit
	luaL_newmetatable(L, DSS_GLOBALS_MT);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, &DSS_clearstateglobals);
	lua_settable(L, -3);
	lua_pop(L,1);

	// get or create the stateglobals
	g = DSS_getstateglobals(L, &errcode);
	if (errcode == DSS_ERR_NO_GLOBALS)
	{
		// this is expected ! there shouldn't be a record yet on startup
		g = DSS_newstateglobals(L, &errcode);	// create a new one
		if (errcode != DSS_SUCCESS)
		{
			if (errcode == DSS_ERR_OUT_OF_MEMORY)
				return luaL_error(L, "Out of memory: DSS failed to create a global data structure for the LuaState");
			if (errcode == DSS_ERR_INIT_MUTEX_FAILED)
				return luaL_error(L, "Mutex init failed: DSS failed to create a global data structure for the LuaState");
			return luaL_error(L, "Unknown error occured while trying to create a global data structure for the LuaState");
		}
	}
	g->DSS_status = DSS_STATUS_STARTED;

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
	DSS_mutexLockx(&statelock);
	if (statecount == 0)
	{
		DSS_networkInit();
	}
	statecount = statecount + 1;
	DSS_mutexUnlockx(&statelock);

	luaL_register(L,"darksidesync",DarkSideSync);
#ifdef _DEBUG
OutputDebugStringA("DSS: LuaOpen completed\n");
#endif
	return 1;
};


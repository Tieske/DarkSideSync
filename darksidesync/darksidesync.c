#include <stdlib.h>
#include <lauxlib.h>
#include "udpsocket.h"
#include "locking.h"
#include "delivery.h"
#include "darksidesync.h"

static putilRecord volatile UtilStart = NULL;		// Holds first utility in the list
static void* volatile DSS_initialized = NULL;		// while its NULL, the first mutex is uninitialized
static DSS_mutex_t dsslock;							// lock for all shared DSS access
static int statecount = 0;							// counter for number of lua states using this lib
//static DSS_mutex_t statelock;						// lock to protect the state counter
static DSS_api_1v0_t DSS_api_1v0;					// API struct for version 1.0

// forward definitions
static void setUDPPort (pglobalRecord g, int newPort);

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
// DSS_SUCCESS, DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NO_GLOBALS
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
		g->socket = udpsocket_new(g->udpport);
		g->DSS_status = DSS_STATUS_STOPPED;

		// setup data queue
		//if (DSS_mutexInitx(&(g->lock)) != 0) *errcode = DSS_ERR_INIT_MUTEX_FAILED;
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
	if (g == NULL || DSS_isvalidglobals(g) == 0) 
	{
		// following error call will not return
		luaL_error(L, "DSS was not started yet, or already stopped");
	}
	return g;
}

// Garbage collect function for the global userdata
// DSS is exiting from this LuaState, so clean it all up
static int DSS_clearstateglobals(lua_State *L)
{
	pglobalRecord g;
	putilRecord listend;

	DSS_mutex_lock(&dsslock);
	g = (pglobalRecord)lua_touserdata(L, 1);		// first param is userdata to destroy

#ifdef _DEBUG
	OutputDebugStringA("DSS: Unloading DSS ...\n");
#endif
	// Set status to stopping, registering and delivering will fail from here on
	g->DSS_status = DSS_STATUS_STOPPING;
	
	// cancel all utilities, in reverse order
	while (UtilStart != NULL)
	{
		listend = UtilStart;
		while (listend->pNext != NULL) listend = listend->pNext;

		DSS_mutex_unlock(&dsslock);		// must unlock to let the cancel function succeed
		listend->pCancel(listend, listend->pUtilData);	// call this utility's cancel method
		DSS_mutex_lock(&dsslock);		// lock again to get the next one
	}
	
	// update status again, we're done stopping
	g->DSS_status = DSS_STATUS_STOPPED;

	// Remove references from the registry
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

	// Close socket and destroy mutex
	setUDPPort(g, 0);  // set port to 0, will close socket

	// Reduce state count and close network if none left
	statecount = statecount - 1;
	if (statecount == 0)
	{
		udpsocket_networkStop();
	}
	DSS_mutex_unlock(&dsslock);
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

// Pop item from the queue
// Returns queueItem, or NULL if none available
// utilid is id of utility for which to return, or NULL to
// just return the oldest item, independent of a utilid
static pQueueItem queuePopUnlocked(pglobalRecord g, putilRecord utilid)
{
	pQueueItem qi = NULL;

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
			// dismiss item from linked list
			if (qi == g->QueueStart) g->QueueStart = qi->pNext;
			if (qi == g->QueueEnd) g->QueueEnd = qi->pPrevious;
			if (qi->pPrevious != NULL) qi->pPrevious->pNext = qi->pNext;
			if (qi->pNext != NULL) qi->pNext->pPrevious = qi->pPrevious;
			// cleanup results
			qi->pNext = NULL;
			qi->pPrevious = NULL;
			g->QueueCount -= 1;
		}
	}

	return qi; //result;
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
	if (g->udpport != 0)
	{
		udpsocket_close(g->socket); 
	}
	g->udpport = newPort;
	if (newPort != 0)
	{
		g->socket = udpsocket_new(newPort);
	}
}

/*
** ===============================================================
** C API
** ===============================================================
*/
// check utildid against list, 1 if it exists, 0 if not
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
	pQueueItem pqi;
	pDSS_waithandle wh;

#ifdef _DEBUG
	OutputDebugStringA("DSS: Start delivering data ...\n");
#endif

	DSS_mutex_lock(&dsslock);
	if (DSS_validutil(utilid) == 0)
	{
		// invalid ID
		DSS_mutex_unlock(&dsslock);
		return DSS_ERR_INVALID_UTILID;
	}

	if (pDecode == NULL)
	{
		// No decode callback provided
		DSS_mutex_unlock(&dsslock);
		return DSS_ERR_NO_DECODE_PROVIDED;
	}

	g = utilid->pGlobals;	
	if (g->DSS_status != DSS_STATUS_STARTED)
	{
		// lib not started yet (or stopped already), exit
		DSS_mutex_unlock(&dsslock);
		return DSS_ERR_NOT_STARTED;
	}

	// Go and deliver it
	pqi = delivery_new(utilid, pDecode, pReturn, pData, &result);

	// get waithandle and unlock to let the delivery be processed
	wh = pqi->pWaitHandle;
	pqi = NULL;  // let go here, after the lock is released, we can no longer assume it valid
	DSS_mutex_unlock(&dsslock);

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

	DSS_mutex_lock(&dsslock);	
	if (DSS_validutil(utilid))
	{
		result = utilid->pUtilData;
	}
	else
	{
		*errcode = DSS_ERR_INVALID_UTILID;
		result = NULL;
	}
	DSS_mutex_unlock(&dsslock);
	return result;
}

// Sets the data associated with the Utility
// Return DSS_SUCCESS or DSS_ERR_INVALID_UTILID
static int DSS_setdata_1v0(putilRecord utilid, void* pData)
{
	int result = DSS_SUCCESS;
	DSS_mutex_lock(&dsslock);	
	if (DSS_validutil(utilid))
	{
		utilid->pUtilData = pData;
	}
	else
	{
		result = DSS_ERR_INVALID_UTILID;
	}
	DSS_mutex_unlock(&dsslock);
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
		DSS_mutex_lock(&dsslock);
		if (g->DSS_status != DSS_STATUS_STARTED)
		{
			DSS_mutex_unlock(&dsslock);
			*errcode = DSS_ERR_NOT_STARTED;
			return NULL;
		}

		// we've got a set of globals, now compare this to the utillist
		utilid = UtilStart;
		while (utilid != NULL)
		{
			if ((utilid->pGlobals == g) && (utilid->libid == libid))
			{
				//This utilid matches both the LuaState (globals) and the libid.
				DSS_mutex_unlock(&dsslock);
				return utilid;	// found it, return and exit.
			}

			// No match, try next one in the list
			utilid = utilid->pNext;
		}
		DSS_mutex_unlock(&dsslock);
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
// NOTE: if the lib was already registered, it will return the existing ID, 
//       but it will ignore all provided parameters (nothing will be changed)
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

	util = (putilRecord)DSS_getutilid_1v0(L, libid, NULL);
	if (util != NULL)
	{
		// We got an ID returned, so this lib is already registered
		*errcode = DSS_ERR_ALREADY_REGISTERED;
		return util;	// don't change anything, but return existing id
	}

	DSS_mutex_lock(&dsslock);
	g = DSS_getstateglobals(L, NULL); 
	if (g == NULL)
	{
		*errcode = DSS_ERR_NOT_STARTED;
		DSS_mutex_unlock(&dsslock);
		return NULL;
	}

	if (g->DSS_status != DSS_STATUS_STARTED)
	{
		// DSS isn't running
		*errcode = DSS_ERR_NOT_STARTED;
		DSS_mutex_unlock(&dsslock);
		return NULL; 
	}

	// create and fill utility record
	util = (putilRecord)malloc(sizeof(utilRecord));
	if (util == NULL) 
	{
		DSS_mutex_unlock(&dsslock);
		*errcode = DSS_ERR_OUT_OF_MEMORY;
		return NULL; 
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

	DSS_mutex_unlock(&dsslock);
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
	//pQueueItem nqi = NULL;
	pQueueItem pqi = NULL;

#ifdef _DEBUG
	OutputDebugStringA("DSS: Start unregistering lib ...\n");
#endif

	DSS_mutex_lock(&dsslock);

	if (DSS_validutil(utilid) == 0)
	{
		// invalid ID
		DSS_mutex_unlock(&dsslock);
		return DSS_ERR_INVALID_UTILID;
	}
	g = utilid->pGlobals;

	// remove it from the list
	if (UtilStart == utilid) UtilStart = utilid->pNext;
	if (utilid->pNext != NULL) utilid->pNext->pPrevious = utilid->pPrevious;
	if (utilid->pPrevious != NULL) utilid->pPrevious->pNext = utilid->pNext;

	// Cancel all items stored in userdatas
	pqi = g->UserdataStart;
	while (pqi != NULL)
	{
		if (pqi->utilid == utilid)
		{
			// need to cancel this one, as it has our ID
			delivery_cancel(pqi);
			// restart as list has been modified...
			pqi = g->UserdataStart;
		}
		else
		{
			// move to next item in list
			pqi = pqi->pNext;
		}
	}

	// cancel all items still in the queue
	pqi = g->QueueEnd;
	while (pqi != NULL)
	{
		if (pqi->utilid == utilid)
		{
			// need to cancel this one, as it has our ID
			delivery_cancel(pqi);
			// restart as list has been modified...
			pqi = g->QueueEnd;
		}
		else
		{
			// move to next item in list
			pqi = pqi->pPrevious;
		}
	}

	// free resources
	free(utilid);

	// Unlock, we're done with the util list
	DSS_mutex_unlock(&dsslock);
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
		DSS_mutex_lock(&dsslock);
		setUDPPort(g, luaL_checkint(L,1));
		DSS_mutex_unlock(&dsslock);
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
	DSS_mutex_lock(&dsslock);
	lua_pushinteger(L, g->udpport);
	DSS_mutex_unlock(&dsslock);
	return 1;
};


// Lua function to get the next item from the queue, its decode
// function will be called to do what needs to be done
// returns: 
// 1st: queuesize of remaining items (or -1 if there was nothing on the queue to begin with)
// 2nd: lua callback function to handle the data
// 3rd: userdata waiting for the response (only of a 'return' call is still valid)
// 4th+: any stuff left by decoder after the callback function (2nd above)
static int L_poll(lua_State *L)
{
	pglobalRecord g = DSS_getvalidglobals(L); // won't return on error
	int result = 0;

	lua_settop(L, 0);		// clear stack

	DSS_mutex_lock(&dsslock);
	if (g->QueueCount > 0)
	{
		// Go decode oldest item
		result = delivery_decode(g->QueueStart, L);
	}
	else
	{
		// Nothing in queue
		lua_pushinteger(L, -1);	// return -1 to indicate queue was empty when called
		result = 1;
	}

	DSS_mutex_unlock(&dsslock);
	return result;
};

// Lua function to get the size of the queue
// returns: 
// 1st: queuesize of remaining items
static int L_queuesize(lua_State *L)
{
	pglobalRecord g = DSS_getvalidglobals(L); // won't return on error
	lua_settop(L, 0);		// clear stack
	DSS_mutex_lock(&dsslock);
	lua_pushinteger(L, g->QueueCount);
	DSS_mutex_unlock(&dsslock);
	return 1;
};

// Execute the return callback, either regular or from garbage collector
static int L_return_internal(lua_State *L, BOOL garbage)
{
	int result = 0;
	pQueueItem pqi = NULL;
	pQueueItem* rqi = (pQueueItem*)luaL_checkudata(L, 1, DSS_QUEUEITEM_MT);	// first item must be our queue item

	DSS_mutex_lock(&dsslock);
	pqi = *rqi;
	if (pqi != NULL)	result = delivery_return(pqi, L, garbage);
	DSS_mutex_unlock(&dsslock);
	return result;
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
	{"queuesize",L_queuesize},
	{NULL,NULL}
};

DSS_API	int luaopen_darksidesync(lua_State *L)
{
	pglobalRecord g;
	int errcode;

#ifdef _DEBUG
OutputDebugStringA("DSS: LuaOpen started...\n");
#endif

	if (DSS_initialized == NULL)  // first initialization of first mutex, unsafe?
	{
		// Initialize all statics
		if (DSS_mutex_init(&dsslock) != 0)
		{
			// an error occured while initializing the 2 global mutexes
			return luaL_error(L,"DSS had an error initializing its mutexes (utillock)");
		}
		DSS_initialized = &luaopen_darksidesync; //point to 'something', no longer NULL

		// Initializes API structure for API 1.0 (static, so only once)
		DSS_api_1v0.version = DSS_API_1v0_KEY;
		DSS_api_1v0.reg = (DSS_register_1v0_t)&DSS_register_1v0;
		DSS_api_1v0.getutilid = (DSS_getutilid_1v0_t)&DSS_getutilid_1v0;
		DSS_api_1v0.deliver = (DSS_deliver_1v0_t)&DSS_deliver_1v0;
		DSS_api_1v0.getdata = (DSS_getdata_1v0_t)&DSS_getdata_1v0;
		DSS_api_1v0.setdata = (DSS_setdata_1v0_t)&DSS_setdata_1v0;
		DSS_api_1v0.unreg = (DSS_unregister_1v0_t)&DSS_unregister_1v0;
	}

	// Create metatable for userdata's waiting for 'return' callback
	luaL_newmetatable(L, DSS_QUEUEITEM_MT);
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);  
	lua_settable(L, -3);	// copy metatable itself
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, &L_queueItemGC);
	lua_settable(L, -3);
	lua_pushstring(L, "setresult");		// used to be called 'return' but thats a reserved word in Lua
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
	DSS_mutex_lock(&dsslock);
	if (statecount == 0)
	{
		udpsocket_networkInit();
	}
	statecount = statecount + 1;
	DSS_mutex_unlock(&dsslock);

	luaL_register(L,"darksidesync",DarkSideSync);
#ifdef _DEBUG
OutputDebugStringA("DSS: LuaOpen completed\n");
#endif
	return 1;
};


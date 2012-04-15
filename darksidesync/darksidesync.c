#include <stdlib.h>
#include <lauxlib.h>
#include "udpsocket.h"
#include "locking.h"
#include "darksidesync.h"

// Define symbol for last queue item, independent of utilid
#define DSS_LASTITEM -1

// Symbols for library status 
#define DSS_STATUS_STARTED -1
#define DSS_STATUS_STOPPING -2
#define DSS_STATUS_STOPPED -3

// Lua registry key for lightuserdata to globaldata structure
#define DSS_GLOBALS_KEY "DSSglobals"

// holds port for notification, or 0 for no notification
static int volatile DSS_UDPPort;	// use lock before modifying !!

// Structure for storing information in the queue
typedef struct qItem *pqueueItem;
typedef struct qItem {
		long utilid;				// unique ID to utility
		DSS_decoder_1v0_t pDecode;	// Pointer to the decode function
		void* pData;				// Data to be decoded
		pqueueItem pNext;			// Next item in queue
		pqueueItem pPrevious;		// Previous item in queue
	} queueItem;

// structure for registering utilities
typedef struct utilReg *putilRecord;
typedef struct utilReg {
// TODO: utilid shouldn't be an int, but a void* pointing to the util specific record, its simpler
		int utilid;				// unique ID to utility
		DSS_cancel_1v0_t pCancel;	// pointer to cancel function
		putilRecord pNext;			// Next item in list
		putilRecord pPrevious;		// Previous item in list
// TODO: add a pointer to the struct with the stateglobal data, containing all previous static variables
	} utilRecord;

static pqueueItem volatile QueueStart = NULL;		// Holds first element in the queue
static pqueueItem volatile QueueEnd = NULL;			// Holds the last item in the queue
static int volatile QueueCount = 0;					// Count of items in queue
static putilRecord volatile UtilStart = NULL;		// Holds first utility in the list
static int volatile utilitycount = 0;				// Counter for registering utilities with unique IDs
static int volatile DSS_status = DSS_STATUS_STOPPED;	// Status of library, locked by lockUtilList()
static DSS_api_1v0_t DSS_api_1v0;					// API struct for version 1.0

// TODO: add a status; running, stopping, stopped, including errors when attempting to register/deliver while stopping/stopped
#ifdef WIN32
	#define DSS_API __declspec(dllexport)
#else
	#define DSS_API extern
#endif

/*
** ===============================================================
** Utility and globals registration functions
** ===============================================================
*/
	// Lookup a utility record for the given utilid
	// NOTE: will not lock the register, must be done before calling!!
	// Returns: utility record for the utilid, or NULL if not found
	putilRecord getUtility(int utilid)
	{
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
		//TODO: implement, should it be a lightuserdata, or fulluserdata?
		//TODO: use DSS_GLOBALS_KEY to collect/store structure
	}

/*
** ===============================================================
** Queue management functions
** ===============================================================
*/
	// Push item in the queue
	// Returns number of items in queue, or DSS_ERR_OUT_OF_MEMORY if it failed
	int queuePush (int utilid, DSS_decoder_1v0_t pDecode, void* pData)
	{
		pqueueItem pqi = NULL;
		int cnt;

		if (NULL == (pqi = malloc(sizeof(queueItem))))
			return DSS_ERR_OUT_OF_MEMORY;	// exit, memory alloc failed

		(*pqi).utilid = utilid;
		(*pqi).pDecode = pDecode;
		(*pqi).pData = pData;
		(*pqi).pNext = NULL;
		(*pqi).pPrevious = NULL;

		lockQueue();
		if (QueueStart == NULL)
		{
			// first item in queue
			QueueStart = pqi;
			QueueEnd = pqi;
		}
		else
		{
			// append to queue
			(*QueueEnd).pNext = pqi;
			(*pqi).pPrevious = QueueEnd;
			QueueEnd = pqi;
		}
		QueueCount += 1;
		cnt = QueueCount;
		unlockQueue();

		return cnt;
	}

	// Pop item from the queue
	// Returns queueItem filled, or all NULLs if none available
	// utilid is id of utility for which to return, or DSS_LASTITEM to
	// just return the last item, independent of a utilid
	queueItem queuePop (long utilid)
	{
		pqueueItem qi;
		queueItem result;
		result.utilid = DSS_LASTITEM;
		result.pData = NULL;
		result.pDecode = NULL;
		result.pNext = NULL;
		result.pPrevious = NULL;

		
		lockQueue();
		if (QueueStart != NULL)
		{

			if (utilid == DSS_LASTITEM)
			{
				// just grab the last one from the queue
				qi = QueueStart;
			}
			else
			{
				// traverse backwards to find the last one with a corresponding utilid
				qi = QueueEnd;
				while (qi != NULL && (*qi).utilid != utilid)
				{
					qi = (*qi).pPrevious;
				}
			}

			if (qi != NULL) 
			{
				result = (*qi);
				// dismiss item from linked list
				if (result.pPrevious != NULL) (*result.pPrevious).pNext = result.pNext;
				if (result.pNext != NULL) (*result.pNext).pPrevious = result.pPrevious;
				free(qi);		// release queueItem memory
				// cleanup results
				result.pNext = NULL;
				result.pPrevious = NULL;
				QueueCount -= 1;
			}
		}
		unlockQueue();

		return result;
	}

/*
** ===============================================================
** UDP socket management functions
** ===============================================================
*/
	// Changes the UDP port number in use
	void setUDPPort (int newPort)
	{
		lockSocket();
		if (DSS_UDPPort != 0)
		{
			destroySocket(); 
		}
		DSS_UDPPort = newPort;
		if (DSS_UDPPort != 0)
		{
			createSocket(DSS_UDPPort);
		}
		unlockSocket();	
	}

	// Gets the UDP port number in use
	int getUDPPort ()
	{
		int s;
		lockSocket();
		s = DSS_UDPPort;
		unlockSocket();
		return s;
	}

/*
** ===============================================================
** C API
** ===============================================================
*/

	// Call this to deliver data to the queue
	// the memory allocated for pData will be released by
	// DSS after it has called the pDecode function (from the
	// Lua API poll() function).
	// @returns; DSS_SUCCESS, DSS_ERR_INVALID_UTILID, DSS_ERR_UDP_SEND_FAILED, 
	// DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NOT_STARTED
	int DSS_deliver_1v0 (int utilid, DSS_decoder_1v0_t pDecode, void* pData)
	{
		int result = DSS_SUCCESS;	// report success by default
		int cnt;
		char buff[6];

		lockUtilList();
		if (DSS_status != DSS_STATUS_STARTED)
		{
			unlockUtilList();
			return DSS_ERR_NOT_STARTED;
		}
		unlockUtilList();

		if (getUtility(utilid) == NULL)
		{
			return DSS_ERR_INVALID_UTILID;
		}

		cnt = queuePush(utilid, pDecode, pData);	// Push it on the queue
		if (cnt == DSS_ERR_OUT_OF_MEMORY) return DSS_ERR_OUT_OF_MEMORY;

		sprintf(buff, " %d", cnt);	// convert to string
		
		// Now send notification packet
		lockSocket();
		if (DSS_UDPPort != 0)
		{
			if (sendPacket(buff) == 0)
			{
				// sending failed, retry
				destroySocket(); 
				createSocket(DSS_UDPPort);
				if (sendPacket(buff) == 0)
					result = DSS_ERR_UDP_SEND_FAILED;		// report failure
			};
		}
		unlockSocket();
		
		return result;	
	};

	// register a library to use DSS 
	// @arg1; pointer to the cancel method of the utility, will be called
	// when DSS decides to terminate the collaboration with the utility
	// Returns: unique ID for the utility that must be used for all subsequent
	// calls to DSS (1 or greater)
	int DSS_register_1v0(DSS_cancel_1v0_t pCancel)
	{
		int newid;
		putilRecord util;
		putilRecord last;

		lockUtilList();
		if (DSS_status != DSS_STATUS_STARTED)
		{
			unlockUtilList();
			return DSS_ERR_NOT_STARTED;
		}
		unlockUtilList();

		if (pCancel == NULL)
		{
			return DSS_ERR_NO_CANCEL_PROVIDED;
		}

		lockUtilList();
		// find a unique ID that is > 0
		while (utilitycount < 1 || getUtility(utilitycount) != NULL)
		{
			utilitycount += 1;
			if (utilitycount < 1) utilitycount = 1;
		}
		newid = utilitycount;
		utilitycount += 1;

		// create and fill utility record
		util = malloc(sizeof(utilRecord));
		if (util == NULL) return DSS_ERR_OUT_OF_MEMORY;
		(*util).pCancel = pCancel;
		(*util).utilid = newid;
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
		return newid;
	}


	// unregisters a previously registered utility
	// Error if it doesn't exits
	// returns DSS_SUCCESS, DSS_ERR_INVALID_UTILID
	int DSS_unregister_1v0(int utilid)
	{
		queueItem qi;
		putilRecord util = NULL;
		lockUtilList();
		util = getUtility(utilid);
		if (util == NULL)
		{
			unlockUtilList();
			return DSS_ERR_INVALID_UTILID; 
		}
		else
		{
			// remove it from the list
			if (UtilStart == util) UtilStart = (*util).pNext;
			if ((*util).pNext != NULL) (*(*util).pNext).pPrevious = (*util).pPrevious;
			if ((*util).pPrevious != NULL) (*(*util).pPrevious).pNext = (*util).pNext;
			free(util);
			// Unlock, we're done with the util list
			unlockUtilList();
			// cancel all items still in the queue
			qi = queuePop(utilid);
			while (qi.pDecode != NULL)
			{
				qi.pDecode(NULL, qi.pData);	// no LuaState, use NULL to indicate cancelling
				qi = queuePop(utilid);		// get next one
			}
		}
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
			setUDPPort(luaL_checkint(L,1));
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

	// Lua function to stop the library 
	// will cancel all registered utilities.
	int L_stop(lua_State *L)
	{
		putilRecord listend = NULL;

		// Clear pointer to DSS api table in the Lua registry
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

		lockUtilList();
		DSS_status = DSS_STATUS_STOPPING;

		// cancel all utilities, in reverse order
		while (UtilStart != NULL)
		{
			listend = UtilStart;
			while ((*listend).pNext != NULL) listend = (*listend).pNext;

			unlockUtilList();
			(*listend).pCancel();	// call this utility cancel method
			lockUtilList();
		}
		
		DSS_status = DSS_STATUS_STOPPED;
		unlockUtilList();
		return 0;
	}

	// Lua function to get the UDP port number in use
	// @luareturns: UDP portnumber 1-65535 in use, or 0 if no port
	int L_getport (lua_State *L)
	{
		lua_settop(L,0);
		lockSocket();
		lua_pushinteger(L, DSS_UDPPort);
		unlockSocket();
		return 1;
	};

	// Lua function to get the next item from the queue or
	// nil if none available
	int L_poll(lua_State *L)
	{
		queueItem qi = queuePop(DSS_LASTITEM);
		int cnt = 0;
		lua_settop(L,0);		// drop any argument provided

		if (qi.pDecode != NULL)
		{
			// Call the decoder function with the data provided
			qi.pDecode(L, qi.pData);
			qi.pData = NULL;
			lua_settop(L,0);		// drop any arguments returned
			lockQueue();
			lua_pushinteger(L, QueueCount);	// return current queue size
			unlockQueue();
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
//TODO: add a garbage collector that stops the queue and clients		

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
		// get or create the stateglobals
		DSS_getstateglobals(L);

		lockUtilList();
		DSS_status = DSS_STATUS_STARTED;
		unlockUtilList();

		luaL_register(L,"darksidesync",DarkSideSync);
		return 1;
	};


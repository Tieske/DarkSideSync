#include <stdlib.h>
#include <lauxlib.h>
#include "udpsocket.h"
#include "locking.h"
#include "darksidesync.h"

// holds port for notification, or 0 for no notification
static int volatile DSS_UDPPort;	// use lock before modifying !!

// Structure for storing information in the queue
typedef struct qItem *pqueueItem;
typedef struct qItem {
		DSS_decoder_t pDecode;		// Pointer to the decode function
		void* pData;				// Data to be decoded
		pqueueItem pNext;			// Next item in queue
	} queueItem;

static pqueueItem volatile QueueStart = NULL;		// Holds first element in the queue
static pqueueItem volatile QueueEnd = NULL;			// Holds the last item in the queue
static int volatile QueueCount = 0;					// Count of items in queue


/*
** ===============================================================
** Queue management functions
** ===============================================================
*/
	// Push item in the queue
	// Returns number of items in queue, or -1 if it failed
	int queuePush (DSS_decoder_t pDecode, void* pData)
	{
		pqueueItem pqi = NULL;
		int cnt;

		if (NULL == (pqi = malloc(sizeof(queueItem))))
			return (-1);	// exit, memory alloc failed

		(*pqi).pDecode = pDecode;
		(*pqi).pData = pData;
		(*pqi).pNext = NULL;

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
			QueueEnd = pqi;
		}
		QueueCount += 1;
		cnt = QueueCount;
		unlockQueue();

		return cnt;
	}

	// Pop item from the queue
	// Returns queueItem filled, or all NULLs if none available
	queueItem queuePop ()
	{
		queueItem qi;
		qi.pDecode = NULL;
		qi.pData = NULL;
		qi.pNext = NULL;
		
		lockQueue();
		if (QueueStart != NULL)
		{
			qi = *QueueStart;
			free(QueueStart);		// release queueItem memory
			QueueStart = qi.pNext;
			if (QueueStart == NULL) QueueEnd = NULL; //last item collected
			qi.pNext = NULL;
		}
		unlockQueue();

		return qi;
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
	// @returns; 0 on error sending UDP packet, 1 otherwise
	int DSS_deliver (DSS_decoder_t pDecode, void* pData)
	{
		int result = 1;	// report success by default
		int cnt = queuePush(pDecode, pData);	// Push it on the queue
		char buff[6];
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
					result = 0;		// report failure
			};
		}
		unlockSocket();
		
		return result;	
	};

/*
** ===============================================================
** Lua API
** ===============================================================
*/
	// Lua function to start the library and initially set the UDP port
	// @luaparam; nil, or UDP port number to use
	// @luareturns; 1 if successfull, or nil + error msg
	int L_start(lua_State *L)
	{
		int newPort;

		if (lua_gettop(L) >= 1 && lua_isnumber(L,1))
		{
			newPort = luaL_checkint(L,1); // returns 0 if it fails

			if (newPort < 0 || newPort > 65535)
			{
				// Port number outside valid range
				lua_settop(L,0);
				lua_pushnil(L);
				lua_pushstring(L, "Invalid port number, use an integer value from 0 to 65535");
				return 2;
			}
			setUDPPort(newPort);
		}
		else
		{
			// There are no parameters, or the first isn't a number
			lua_settop(L,0);
			lua_pushnil(L);
			lua_pushstring(L, "Invalid port number, use an integer value from 0 to 65535");
			return 2;
		}

		// Store pointer to my 'deliver' function in the Lua registry
		// for backgroundworkers to collect there
		// TODO: change to table, with sub tables. Versions as keys
		// and table content being version specific
		lua_pushlightuserdata(L,&DSS_deliver);
		lua_setfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);

		// report success
		lua_settop(L,0);
		lua_pushinteger(L, 1);
		return 1;
	};

	// Lua function to get the UDP port number in use
	// @luareturns: UDP portnumber 0-65535 in use
	int L_getport (lua_State *L)
	{
		lua_settop(L,0);
		lockSocket();
		lua_pushinteger(L,DSS_UDPPort);
		unlockSocket();
		return 1;
	};

	// Lua function to get the next item from the queue or
	// nil if none available
	int L_poll(lua_State *L)
	{
		int cnt = 0;
		lua_settop(L,0);		// drop any argument provided

		queueItem qi = queuePop();
		if (qi.pDecode != NULL)
		{
			// Call the decoder function with the data provided
			cnt = qi.pDecode(L, qi.pData);
			// Release allocated data memory
			free(qi.pData);
			qi.pData = NULL;
		}
		else
		{
			// No data in queue, return nil
			lua_pushnil(L);
			cnt = 1;
		}
		return cnt;	
	};

/*
** ===============================================================
** Library initialization
** ===============================================================
*/
	static const struct luaL_Reg DarkSideSync[] = {
		{"start",L_start},
		{"poll",L_poll},
		{"getport",L_getport},
		{NULL,NULL}
	};

	int luaopen_darksidesync(lua_State *L){

		if (initLocks() != 0)
		{
			// Mutexes could not be created
			luaL_error(L, "Mutexes could not be created"); // call never returns
		}
		
		lockSocket();
		DSS_UDPPort = 0;
		unlockSocket();
		luaL_register(L,"darksidesync",DarkSideSync);
		return 1;
	};


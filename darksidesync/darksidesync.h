
#ifndef darksidesync_h
#define darksidesync_h

#include <lua.h>
#include "darksidesync_api.h"
#include "udpsocket.h"
#include "locking.h"
#include "waithandle.h"

//////////////////////////////////////////////////////////////
// symbol list												//
//////////////////////////////////////////////////////////////

// Define symbol for last queue item, independent of utilid
#define DSS_LASTITEM NULL

// Symbols for library status 
#define DSS_STATUS_STARTED -1
#define DSS_STATUS_STOPPING -2
#define DSS_STATUS_STOPPED -3

// Lua registry key for globaldata structure
#define DSS_GLOBALS_KEY "DSS.globals"
// Lua registry key for metatable of the global structure userdata
#define DSS_GLOBALS_MT "DSS.globals.mt"
// Lua registry key for metatable of queueItems waiting for 'return' callback
#define DSS_QUEUEITEM_MT "DSS.queueitem.mt"

// Define platform specific extern statement
#ifdef WIN32
	#define DSS_API __declspec(dllexport)
#else
	#define DSS_API extern
#endif

// Windows BOOL compatibility for other platforms
#ifndef BOOL
    #define BOOL int
    #define TRUE 1
    #define FALSE 0
#endif

//////////////////////////////////////////////////////////////
// C side structures for registration and globals			//
//////////////////////////////////////////////////////////////

// first forward declare these to resolve circular references
typedef struct utilReg *putilRecord;
typedef struct qItem *pQueueItem;
typedef struct stateGlobals *pglobalRecord;

// structure for registering utilities
typedef struct utilReg {
		DSS_cancel_1v0_t pCancel;	// pointer to cancel function
		putilRecord pNext;			// Next item in list
		putilRecord pPrevious;		// Previous item in list
		pglobalRecord pGlobals;		// pointer to the global data for this utility
		void* libid;				// unique library specific ID
	} utilRecord;

// Structure for storing data from an async callback in the queue
// NOTE: while waiting for 'poll' to be called it will be in the queue,
//       while waiting for 'return' callback, it will be in a userdata
typedef struct qItem {
		putilRecord utilid;			// unique ID to utility
		pDSS_waithandle pWaitHandle; // Wait handle to block thread while wait for return to be called
		void* pData;				// Data to be decoded
		pQueueItem pNext;			// Next item in queue/list
		pQueueItem pPrevious;		// Previous item in queue/list
		pQueueItem* udata;			// a userdata containing a pointer to this qItem
		// API functions at the end, so casting of future versions can be done
		DSS_decoder_1v0_t pDecode;	// Pointer to the decode function, if NULL then it was already called
		DSS_return_1v0_t pReturn;	// Pointer to the return function
	} QueueItem;

// structure for state global variables to be stored outside of the LuaState
// this is required to be able to access them from an async callback
// (which cannot call into lua to collect global data there)
typedef struct stateGlobals {
		//DSS_mutex_t lock;					// lock to protect struct data
		int volatile udpport;				// 0 = no notification
		udpsocket_t socket;				// structure with socket data
		int volatile DSS_status;			// Status of library
		// Elements for the async data queue
		pQueueItem volatile QueueStart;		// Holds first element in the queue
		pQueueItem volatile QueueEnd;		// Holds the last item in the queue
		int volatile QueueCount;			// Count of items in queue
		// Elements for the userdata list
		pQueueItem volatile UserdataStart;  // Holds first element in the list
	} globalRecord;


#endif /* darksidesync_h */


#ifndef darksidesync_h
#define darksidesync_h

#include <lua.h>
#include "darksidesync_api.h"

//////////////////////////////////////////////////////////////
// symbol list												//
//////////////////////////////////////////////////////////////

// Define symbol for last queue item, independent of utilid
#define DSS_LASTITEM -1

// Symbols for library status 
#define DSS_STATUS_STARTED -1
#define DSS_STATUS_STOPPING -2
#define DSS_STATUS_STOPPED -3

// Lua registry key for lightuserdata to globaldata structure
#define DSS_GLOBALS_KEY "DSSglobals"

// Define platform specific extern statement
#ifdef WIN32
	#define DSS_API __declspec(dllexport)
#else
	#define DSS_API extern
#endif

//////////////////////////////////////////////////////////////
// C side structures for registration and globals			//
//////////////////////////////////////////////////////////////

// first forward declare these to resolve circular references
typedef struct utilReg *putilRecord;
typedef struct qItem *pqueueItem;
typedef struct stateGlobals *pglobalRecord;

// structure for registering utilities
typedef struct utilReg {
		//int utilid;				// unique ID to utility   --- replaced by pointer to this record
		DSS_cancel_1v0_t pCancel;	// pointer to cancel function
		putilRecord pNext;			// Next item in list
		putilRecord pPrevious;		// Previous item in list
		pglobalRecord pGlobals;		// pointer to the global data for this utility
	} utilRecord;

// Structure for storing data from an async callback in the queue
typedef struct qItem {
		putilRecord utilid;			// unique ID to utility
		DSS_decoder_1v0_t pDecode;	// Pointer to the decode function
		void* pData;				// Data to be decoded
		pqueueItem pNext;			// Next item in queue
		pqueueItem pPrevious;		// Previous item in queue
	} queueItem;

// structure for state global variables to be stored outside of the LuaState
// this is required to be able to access them from an async callback
// (which cannot call into lua to collect global data there)
typedef struct stateGlobals {
		DSS_mutex_t lock;					// lock to protect struct data
		// holds port for notification, or 0 for no UDP notification
		int volatile udpport;				// use lock for access!
		DSS_socket_t socket;				// structure with socket data, use lock!
		// Elements for the async data queue
		pqueueItem volatile QueueStart;		// Holds first element in the queue
		pqueueItem volatile QueueEnd;		// Holds the last item in the queue
		int volatile QueueCount;			// Count of items in queue
		int volatile DSS_status;			// Status of library
	} globalRecord;


#endif /* darksidesync_h */

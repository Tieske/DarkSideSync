
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

// structure for state global variables to be stored outside of the LuaState
// this is required to be able to access them from an async callback
// (which cannot call into lua to collect global data there)
typedef struct stateGlobals *pglobalRecord;
typedef struct stateGlobals {
		// holds port for notification, or 0 for no UDP notification
		int volatile DSS_UDPPort;			// use lock before modifying !!
		// Elements for the async data queue
		pqueueItem volatile QueueStart;		// Holds first element in the queue
		pqueueItem volatile QueueEnd;		// Holds the last item in the queue
		int volatile QueueCount;			// Count of items in queue
		// Lib status
		int volatile DSS_status = DSS_STATUS_STOPPED;	// Status of library, locked by lockUtilList()
		DSS_api_1v0_t DSS_api_1v0;					// API struct for version 1.0

		//putilRecord pNext;					// Next item in list
		//putilRecord pPrevious;				// Previous item in list
	} globalRecord;

// structure for registering utilities
typedef struct utilReg *putilRecord;
typedef struct utilReg {
		//int utilid;				// unique ID to utility   --- replaced by pointer to this record
		DSS_cancel_1v0_t pCancel;	// pointer to cancel function
		putilRecord pNext;			// Next item in list
		putilRecord pPrevious;		// Previous item in list
		pglobalRecord pGlobals;		// pointer to the global data for this utility
	} utilRecord;

// Structure for storing data from an async callback in the queue
typedef struct qItem *pqueueItem;
typedef struct qItem {
		putilRecord utilid;			// unique ID to utility
		DSS_decoder_1v0_t pDecode;	// Pointer to the decode function
		void* pData;				// Data to be decoded
		pqueueItem pNext;			// Next item in queue
		pqueueItem pPrevious;		// Previous item in queue
	} queueItem;


#endif /* darksidesync_h */

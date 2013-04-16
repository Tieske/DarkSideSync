#ifndef delivery_h
#define delivery_h

#include "darksidesync.h"
#include "darksidesync_api.h"
#include <lauxlib.h>
#include <stdlib.h>

// Pointer type to delivery item
//typedef struct qItem *pQueueItem;

// Structure for storing data from an async callback in the queue
// NOTE: while waiting for 'poll' to be called it will be in the queue,
//       while waiting for 'return' callback, it will be in a userdata
//typedef struct qItem {
//		putilRecord utilid;			// unique ID to utility
//		pDSS_waithandle pWaitHandle; // Wait handle to block thread while wait for return to be called
//		void* pData;				// Data to be decoded
//		pQueueItem pNext;			// Next item in queue/list
//		pQueueItem pPrevious;		// Previous item in queue/list
//		pQueueItem* udata;			// a userdata containing a pointer to this qItem
//		// API functions at the end, so casting of future versions can be done
//		DSS_decoder_1v0_t pDecode;	// Pointer to the decode function, if NULL then it was already called
//		DSS_return_1v0_t pReturn;	// Pointer to the return function
//	} QueueItem;

// Methods, see code for more detailed comments
// Create a new item and store it
pQueueItem delivery_new(putilRecord utilid, DSS_decoder_1v0_t pDecode, DSS_return_1v0_t pReturn, void* pData, int* err);
// Execute the poll/decode step, and move to userdata
int delivery_decode(pQueueItem pqi, lua_State *L);
// execute return step and destroy
int delivery_return(pQueueItem pqi, lua_State *L, BOOL garbage);
// cancel the item (either from queue or userdata)
void delivery_cancel(pQueueItem pqi);

#endif /* delivery_h */

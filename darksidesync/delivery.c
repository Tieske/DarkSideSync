#include "delivery.h"

// New constructor
// Creates a element for delivery and places it in the queue, waiting for a
// poll to arrive. Creates the waithandle if required and sends the UDP
// notification if set.
//
// @returns; NULL if it failed
// @err;     DSS_SUCCESS, DSS_ERR_UDP_SEND_FAILED, DSS_ERR_INVALID_UTILID,
//           DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NOT_STARTED
//
// Notes:
//    * if it returns an element, the result may still be a warning (see return codes; errors vs warnings)
//    * Utilid MUST be valid before calling

pQueueItem delivery_new(putilRecord utilid, DSS_decoder_1v0_t pDecode, DSS_return_1v0_t pReturn, void* pData, int* err)
{
	pglobalRecord g;
	int result;
	char buff[20];
	pDSS_waithandle wh = NULL;
	pQueueItem pqi = NULL;

	// if no err provided, set it now.
	if (err == NULL) err = &result;
	*err = DSS_SUCCESS;

	g = utilid->pGlobals;	
	if (g->DSS_status != DSS_STATUS_STARTED)
	{
		// lib not started yet (or stopped already), exit
		*err = DSS_ERR_NOT_STARTED;
		return NULL;
	}

	if (NULL == (pqi = (pQueueItem)malloc(sizeof(QueueItem))))
	{
		*err = DSS_ERR_OUT_OF_MEMORY;
		return NULL;	// exit, memory alloc failed
	}

	if (pReturn != NULL)	// only create waithandle if a return function specified
	{
		wh = DSS_waithandle_create();
		if (wh == NULL)
		{
			// error, resource alloc failed
			free(pqi);
			*err = DSS_ERR_OUT_OF_MEMORY; 
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
	pqi->udata = NULL;

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

	sprintf(buff, " %d", g->QueueCount);	// convert to string
	
	// Now send notification packet
	if (g->udpport != 0)
	{
		if (udpsocket_send(g->socket, buff) == 0)
		{
			// sending failed, retry; close create new and do again
			udpsocket_close(g->socket);
			g->socket = udpsocket_new(g->udpport); 
			if (udpsocket_send(g->socket, buff) == 0)
			{
				*err = DSS_ERR_UDP_SEND_FAILED;	// store failure to report
			}
		}
	}

	return pqi;	
};


// Decoder
// removes an item from the queue and deals with the POLL step.
// the decode callback will be called to do what needs to be done
// returns (on Lua stack): 
// 1st: queuesize of remaining items or;
//      -1 to indicate there was nothing in the queue to begin with
// 2nd: lua callback function to handle the data
// 3rd: table containing all callback arguments with;
//    pos 1 : userdata waiting for the response (only if a 'return' call is still valid)
//    pos 2+: any stuff left by decoder after the callback function (2nd above)
//
// Note: if lua_state == NULL then the item will be cancelled
int delivery_decode(pQueueItem pqi, lua_State *L)
{
	int result = 0;
	pQueueItem* udata = NULL;
	pglobalRecord g = pqi->utilid->pGlobals;

	// Remove item from queue
	if (pqi == g->QueueStart) g->QueueStart = pqi->pNext;
	if (pqi == g->QueueEnd) g->QueueEnd = pqi->pPrevious;
	if (pqi->pPrevious != NULL) pqi->pPrevious->pNext = pqi->pNext;
	if (pqi->pNext != NULL) pqi->pNext->pPrevious = pqi->pPrevious;
	// cleanup results
	pqi->pNext = NULL;
	pqi->pPrevious = NULL;
	g->QueueCount -= 1;

	// execute callback, set to NULL to indicate call is done
	result = pqi->pDecode(L, pqi->pData, pqi->utilid);	
	pqi->pDecode = NULL;				

	if (result < 1 || L == NULL)	// if lua_state == NULL then we're cancelling
	{
		// indicator transaction is complete, do NOT create the userdata and do not call return callback
		pqi->pReturn = NULL;
		if (pqi->pWaitHandle != NULL)
		{
			DSS_waithandle_signal(pqi->pWaitHandle);
			pqi->pWaitHandle = NULL;
		}
		lua_pushinteger(L, g->QueueCount);	// add count to results
		free(pqi); // No need to clear userdata, wasn't created yet in this case
		return 1;					// Only count is returned
	}

	lua_checkstack(L, 3);
	if (pqi->pReturn != NULL)
	{
		// Create userdata to reference the queueitem, because we have a return callback
		udata = (pQueueItem*)lua_newuserdata(L, sizeof(pQueueItem));
		if (udata == NULL)
		{
			// memory allocation error, exit process here
			pqi->pReturn(NULL, pqi->pData, pqi->utilid, FALSE); // call with lua_State == NULL to have it cancelled
			DSS_waithandle_signal(pqi->pWaitHandle);
			free(pqi);
			lua_pushinteger(L, pqi->utilid->pGlobals->QueueCount);	// add count to results
			// push an error to notify of failure???
			return 1;					// Only count is returned
		}
		// Set cross references
		*udata = pqi;		// fill userdata with reference to queueitem
		pqi->udata = udata;	// set reference to userdata in queueitem

		// attach metatable
		luaL_getmetatable(L, DSS_QUEUEITEM_MT);
		lua_setmetatable(L, -2);

		// store in userdata list
		pqi->pNext = g->UserdataStart;
		pqi->pPrevious = NULL;
		if (pqi->pNext != NULL) pqi->pNext->pPrevious = pqi;
		g->UserdataStart = pqi;

		// Move userdata (on top) to 2nd position, directly after the lua callback function
		if (lua_gettop(L) > 2 ) lua_insert(L, 2);
		result = result + 1;		// 1 more result because we added the userdata
	}
	lua_createtable(L, result - 1, 0);			// add a table
	if (lua_gettop(L) > 2 ) lua_insert(L, 2);	// move it into 2nd pos
	while (lua_gettop(L) > 2)					// migrate all callback arguments into the table
	{
			lua_rawseti(L, 2, lua_gettop(L)-2);
	}
	lua_pushinteger(L, g->QueueCount);			// add count to results
	lua_insert(L,1);							// move count to 1st position

	return 3;									// count, callback, table cb arguments
}

// Return destructor
// Execute the return callback, cleanup and finish process
//
// Note: if lua_state == NULL then the item will be cancelled
int delivery_return(pQueueItem pqi, lua_State *L, BOOL garbage)
{
	int result = 0;
	pglobalRecord g = pqi->utilid->pGlobals;

	// Move it off the userdata list
	if (pqi->pPrevious == NULL)
	{
		// its the first item in the list, so we need the global record to update the pointer there
		g->UserdataStart = pqi->pNext;
		if (pqi->pNext != NULL) pqi->pNext->pPrevious = NULL;
	}
	else
	{
		// its somewhere mid-list, just update
		pqi->pPrevious->pNext = pqi->pNext;
		if (pqi->pNext != NULL)	pqi->pNext->pPrevious = pqi->pPrevious;
	}
	pqi->pNext = NULL;
	pqi->pPrevious = NULL;

	// Cleanup userdata
	(*(pqi->udata)) = NULL;	// set reference in userdata to NULL, indicate its done
	if (L != NULL) lua_remove(L, 1);	// remove the userdata from the stack

	// now execute callback, here the utility should release all resources
	result = pqi->pReturn(L, pqi->pData, pqi->utilid, garbage);	

	// Cleanup queueitem
	pqi->pReturn = NULL;
	pqi->pData = NULL;
	if (pqi->pWaitHandle != NULL)
	{
		DSS_waithandle_signal(pqi->pWaitHandle);
		pqi->pWaitHandle = NULL;
	}

	// let go of own resources
	free(pqi);

	return result;
}

// Cancel destructor
// Removes the item from the queue or userdata list and destroys it
// will call the appropriate callback to release client resources
void delivery_cancel(pQueueItem pqi)
{
	if (pqi->udata != NULL)
	{
		// There is a userdata, so its on Lua side
		delivery_return(pqi, NULL, FALSE);
	}
	else
	{
		// No userdata, so must be in queue
		delivery_decode(pqi, NULL);
	}
}

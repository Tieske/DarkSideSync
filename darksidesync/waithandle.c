#ifndef dss_waithandle_c
#define dss_waithandle_c

#include <lua.h>
#include <lauxlib.h>
#include "waithandle.h"

/*
** ===============================================================
**  Create a new waithandle
** ===============================================================
*/
// Returns NULL upon failure
// initial state of the handle returned is 'reset' (Closed)
pDSS_waithandle DSS_waithandle_create()
{
	pDSS_waithandle wh = (pDSS_waithandle)malloc(sizeof(DSS_waithandle_t));
	if (wh == NULL)
	{
		// error allocating memory
		return NULL;
	}
	wh->semaphore = CreateSemaphore(NULL, 0, 1, NULL);
	DSS_waithandle_signal(wh);
	return wh;
}

/*
** ===============================================================
**  Resets the waithandle (closes the gate)
** ===============================================================
*/
void DSS_waithandle_reset(pDSS_waithandle wh)
{
	if (wh != NULL) {
		// to reset, first release by 1, has no effect if already releases
		ReleaseSemaphore(wh->semaphore,1, NULL);
		// now wait 1, effectively reducing to 0 and hence closing
		WaitForSingleObject(wh->semaphore, INFINITE);
	}
}

/*
** ===============================================================
**  Signals the waithandle (opens the gate)
** ===============================================================
*/
void DSS_waithandle_signal(pDSS_waithandle wh)
{
	if (wh != NULL) {
		ReleaseSemaphore(wh->semaphore, 1, NULL);
	}
}

/*
** ===============================================================
**  Waits for the waithandle to be signalled (tries passing the gate)
** ===============================================================
*/
void DSS_waithandle_wait(pDSS_waithandle wh)
{
	if (wh != NULL) {
		WaitForSingleObject(wh->semaphore, INFINITE);
	}
}

/*
** ===============================================================
**  Destroys the waithandle
** ===============================================================
*/
void DSS_waithandle_delete(pDSS_waithandle wh)
{
	if (wh != NULL) {
		// release before destroying
		ReleaseSemaphore(wh->semaphore, 1, NULL);
		CloseHandle(wh->semaphore);
	}
}

#endif

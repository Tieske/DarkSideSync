#ifndef dss_waithandle_c
#define dss_waithandle_c

#include <stdlib.h>
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
#ifdef WIN32
	wh->semaphore = CreateSemaphore(NULL, 0, 1, NULL);
	if (wh->semaphore == NULL) {
		// failed initializing
		free(wh);
		return NULL;
	}
#else
	int rt = sem_init(&(wh->semaphore), 0, 0);
	if (rt != 0 ) {
		// failed initializing
		free(wh);
		return NULL;
	}
#endif
	DSS_waithandle_reset(wh);
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
#ifdef WIN32
		// to reset, first release by 1, has no effect if already released
		ReleaseSemaphore(wh->semaphore,1, NULL);
		// now wait 1, effectively reducing to 0 and hence closing
		WaitForSingleObject(wh->semaphore, INFINITE);
#else
		while (sem_trywait(&(wh->semaphore)) == 0);  // wait (and reduce) until error (value = 0 and blocking)
#endif
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
#ifdef WIN32
		ReleaseSemaphore(wh->semaphore, 1, NULL);
#else
		sem_post(&(wh->semaphore));
#endif
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
#ifdef WIN32
		WaitForSingleObject(wh->semaphore, INFINITE);
#else
		sem_wait(&(wh->semaphore));
#endif
	}
}

/*
** ===============================================================
**  Destroys the waithandle, releases resources
** ===============================================================
*/
void DSS_waithandle_delete(pDSS_waithandle wh)
{
	if (wh != NULL) {
#ifdef WIN32
		// release before destroying
		ReleaseSemaphore(wh->semaphore, 1, NULL);
		CloseHandle(wh->semaphore);
#else
		// release before destroying
		sem_post(&(wh->semaphore));
		sem_destroy(&(wh->semaphore));
#endif
		// release resources
		free(wh);
	}
}

#endif

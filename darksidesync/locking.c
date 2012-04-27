#ifndef dss_locking_c
#define dss_locking_c

#include "locking.h"

/*
** ===============================================================
** Locking functions
** ===============================================================
*/

// Initializes the mutex, returns 0 upon success, 1 otherwise
int DSS_mutexInitx(DSS_mutex_t *m)
{
#ifdef WIN32
	*m = CreateMutex( 
			NULL,              // default security attributes
			FALSE,             // initially not owned
			NULL);             // unnamed mutex
	if (*m == NULL)
		return 1;
	else
		return 0;
#else
	int r = pthread_mutex_init(&m, NULL);	// return 0 upon success
	return r
#endif
}

// Destroy mutex
void DSS_mutexDestroyx(DSS_mutex_t *m)
{
#ifdef WIN32
	CloseHandle(*m);
#else
	pthread_mutex_destroy(&m);
#endif
}

// Locks a mutex
void DSS_mutexLockx(DSS_mutex_t *m)
{
#ifdef WIN32
	WaitForSingleObject(*m, INFINITE);
#else
	pthread_mutex_lock(&m);
#endif
}

// Unlocks a mutex
void DSS_mutexUnlockx(DSS_mutex_t *m)
{
#ifdef WIN32
	ReleaseMutex(*m);
#else
	pthread_mutex_unlock(&m);
#endif
}

#endif

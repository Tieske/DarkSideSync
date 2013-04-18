#ifndef dss_locking_c
#define dss_locking_c

#include "locking.h"


/*
** ===============================================================
** Locking functions
** ===============================================================
*/

// Initializes the mutex, returns 0 upon success, 1 otherwise
int DSS_mutex_init(DSS_mutex_t* m)
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
	// create attribute and set it to RECURSIVE as the mutex type
	pthread_mutexattr_t Attr;
	pthread_mutexattr_init(&Attr);
	pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
	int r = pthread_mutex_init(m, &Attr);	// return 0 upon success
	return r;
#endif
}

// Destroy mutex
void DSS_mutex_destroy(DSS_mutex_t* m)
{
#ifdef WIN32
	CloseHandle(*m);
#else
	pthread_mutex_destroy(m);
#endif
}

// Locks a mutex
void DSS_mutex_lock(DSS_mutex_t* m)
{
#ifdef WIN32
	WaitForSingleObject(*m, INFINITE);
#else
	pthread_mutex_lock(m);
#endif
}

// Unlocks a mutex
void DSS_mutex_unlock(DSS_mutex_t* m)
{
#ifdef WIN32
	ReleaseMutex(*m);
#else
	pthread_mutex_unlock(m);
#endif
}

#endif

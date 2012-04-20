#ifndef dss_locking_h
#define dss_locking_h

#ifdef WIN32
	#include <windows.h>
	#define DSS_mutex_t = HANDLE
#else  // Unix
	#include <pthread.h>
	#define DSS_mutex_t = pthread_mutex_t
#endif

//static DSS_mutex_t SocketMutex;
//static DSS_mutex_t QueueMutex;

/*
** ===============================================================
** Locking functions
** ===============================================================
*/

// Initializes the mutex, returns 0 upon success, 1 otherwise
int DSS_mutexInit(DSS_mutex_t m)
{
#ifdef WIN32
	m = CreateMutex( 
			NULL,              // default security attributes
			FALSE,             // initially not owned
			NULL);             // unnamed mutex
	if (m == NULL)
		return 1
	else
		return 0
#else
	int r = pthread_mutex_init(&m, NULL);	// return 0 upon success
	return r
#endif
}

// Destroy mutex
void DSS_mutexDestroy(DSS_mutex_t m)
{
#ifdef WIN32
	CloseHandle(m);
#else
	pthread_mutex_destroy(&m);
#endif
}

// Locks a mutex
void DSS_mutexLock(DSS_mutex_t m)
{
#ifdef WIN32
	WaitForSingleObject(m, INFINITE);
#else
	pthread_mutex_lock(&m);
#endif
}

// Unlocks a mutex
void DSS_mutexUnlock(DSS_mutex_t m)
{
#ifdef WIN32
	ReleaseMutex(m);
#else
	pthread_mutex_unlock(&m);
#endif
}

#endif

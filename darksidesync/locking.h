#ifndef dss_locking_h
#define dss_locking_h

#ifdef WIN32
	#include <windows.h>
	#define DSS_mutex_t HANDLE
	#define DSS_MUTEX_INITIALIZER
#else  // Unix
	#include <pthread.h>
	#define DSS_mutex_t pthread_mutex_t
	#define DSS_MUTEX_INITIALIZER
#endif

int DSS_mutexInit(DSS_mutex_t m);
void DSS_mutexDestroy(DSS_mutex_t m);
void DSS_mutexLock(DSS_mutex_t m);
void DSS_mutexUnlock(DSS_mutex_t m);

#endif  /* dss_locking_h */

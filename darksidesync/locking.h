#ifndef dss_locking_h
#define dss_locking_h

#ifdef WIN32
	#include <windows.h>
	#define DSS_mutex_t HANDLE
#else  // Unix
	#include <pthread.h>
	#define DSS_mutex_t pthread_mutex_t
#endif

int DSS_mutexInitx(DSS_mutex_t* m);
void DSS_mutexDestroyx(DSS_mutex_t* m);
void DSS_mutexLockx(DSS_mutex_t* m);
void DSS_mutexUnlockx(DSS_mutex_t* m);

#endif  /* dss_locking_h */

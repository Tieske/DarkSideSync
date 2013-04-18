#ifndef dss_waithandle_h
#define dss_waithandle_h

#ifdef WIN32
	#include <Windows.h>
#else
	#include <semaphore.h>
#endif

// waithandle structure
typedef struct DSS_waithandle *pDSS_waithandle;
typedef struct DSS_waithandle {
	#ifdef WIN32
		HANDLE semaphore;	
	#else  // Unix
		sem_t semaphore;
	#endif
} DSS_waithandle_t;


// WaitHandle operations
pDSS_waithandle DSS_waithandle_create();        // creates a new waithandle, initial state is signalled
void DSS_waithandle_reset(pDSS_waithandle wh);  // resets status to blocking (closes the gate)
void DSS_waithandle_signal(pDSS_waithandle wh); // sets status to signalled (opens the gate)
void DSS_waithandle_wait(pDSS_waithandle wh);   // blocks thread until handle gets signalled
void DSS_waithandle_delete(pDSS_waithandle wh); // destroys the waithandle

#endif  /* dss_waithandle_h */

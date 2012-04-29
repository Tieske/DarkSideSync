#ifndef dss_locking_c
#define dss_locking_c

#include "locking.h"

	HANDLE ghMutex;

DWORD WINAPI WriteToDatabase( LPVOID lpParam )
{ 
    // lpParam not used in this example
    //UNREFERENCED_PARAMETER(lpParam);

    DWORD dwCount=0;
	DWORD dwWaitResult; 

    // Request ownership of mutex.

    //while( dwCount < 20 )
    //{ 
        dwWaitResult = WaitForSingleObject( 
            ghMutex,    // handle to mutex
            INFINITE);  // no time-out interval
 
        switch (dwWaitResult) 
        {
            // The thread got ownership of the mutex
            case WAIT_OBJECT_0: 
                __try { 
                    // TODO: Write to the database
                    printf("Thread %d writing to database...\n", 
                            GetCurrentThreadId());
                    dwCount++;
                } 

                __finally { 
                    // Release ownership of the mutex object
                    if (! ReleaseMutex(ghMutex)) 
                    { 
                        // Handle error.
                    } 
                } 
                break; 

            // The thread got ownership of an abandoned mutex
            // The database is in an indeterminate state
            case WAIT_ABANDONED: 
                return FALSE; 
        }
    //}
    return TRUE; 
}

void testmutex()
{
	HANDLE aThread;
	DWORD ThreadID, dwWaitResult;
	ghMutex = CreateMutex( 
			NULL,              // default security attributes
			FALSE,             // initially not owned
			NULL);             // unnamed mutex
	//dwWaitResult = WaitForSingleObject(ghMutex, INFINITE);
	//ReleaseMutex(ghMutex);
	dwWaitResult = WaitForSingleObject(ghMutex, INFINITE);
	aThread = CreateThread( 
                     NULL,       // default security attributes
                     0,          // default stack size
                     (LPTHREAD_START_ROUTINE) WriteToDatabase, 
                     NULL,       // no thread function arguments
                     0,          // default creation flags
                     &ThreadID); // receive thread identifier
	//dwWaitResult = WaitForSingleObject(ghMutex, INFINITE);	// should block...
	//ReleaseMutex(ghMutex);
	//ReleaseMutex(ghMutex);
	//CloseHandle(ghMutex);
	
}

/*
** ===============================================================
** Locking functions
** ===============================================================
*/

// Initializes the mutex, returns 0 upon success, 1 otherwise
int DSS_mutexInitx(DSS_mutex_t* m)
{
	testmutex();
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
void DSS_mutexDestroyx(DSS_mutex_t* m)
{
#ifdef WIN32
	CloseHandle(*m);
#else
	pthread_mutex_destroy(&m);
#endif
}

// Locks a mutex
void DSS_mutexLockx(DSS_mutex_t* m)
{
#ifdef WIN32
	WaitForSingleObject(*m, INFINITE);
#else
	pthread_mutex_lock(&m);
#endif
}

// Unlocks a mutex
void DSS_mutexUnlockx(DSS_mutex_t* m)
{
#ifdef WIN32
	ReleaseMutex(*m);
#else
	pthread_mutex_unlock(&m);
#endif
}

#endif

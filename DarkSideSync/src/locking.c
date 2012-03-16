#ifndef dss_locking_h
#define dss_locking_h

#ifdef WIN32
#include <windows.h>
//#include <stdio.h>
static HANDLE SocketMutex;
static HANDLE QueueMutex;

#else  // Unix

#include <pthread.h>
static pthread_mutex_t SocketMutex = PTHREAD_MUTEX_INITIALIZER;	//Socket protection
static pthread_mutex_t QueueMutex = PTHREAD_MUTEX_INITIALIZER;	//Queue protection

#endif



/*
** ===============================================================
** Locking functions
** ===============================================================
*/
#ifdef WINDOWS

	// Initialize mutexes
	int initLocks ()
	{
		SocketMutex = CreateMutex( 
			NULL,              // default security attributes
			FALSE,             // initially not owned
			NULL);             // unnamed mutex

		if (SocketMutex == NULL) 
		{
			return 1;	// report failure
		}		
		QueueMutex = CreateMutex( 
			NULL,              // default security attributes
			FALSE,             // initially not owned
			NULL);             // unnamed mutex

		if (QueueMutex == NULL) 
		{
			CloseHandle(SocketMutex); // close created mutex andle
			return 1;	// report failure
		}
		return 0;
	}
		

	// Lock the UDP socket and the UDPPort variable
	void lockSocket ()
	{
		WaitForSingleObject(SocketMutex, INFINITE);
	}
	
	// Unlock the UDP socket and the UDPPort variable
	void unlockSocket ()
	{
		ReleaseMutex(SocketMutex);
	}

	// Lock the queue
	void lockQueue ()
	{
		WaitForSingleObject(QueueMutex, INFINITE);
	}
	
	// Unlock the queue
	void unlockQueue ()
	{
		ReleaseMutex(QueueMutex);
	}

#else   // Unix

	// Initialize mutexes
	int initLocks ()
	{
		return 0;	// nothing to do for posix
	}
	
	// Lock the UDP socket and the UDPPort variable
	void lockSocket ()
	{
		pthread_mutex_lock( &SocketMutex );
	}
	
	// Unlock the UDP socket and the UDPPort variable
	void unlockSocket ()
	{
		pthread_mutex_unlock( &SocketMutex );
	}

	// Lock the queue
	void lockQueue ()
	{
		pthread_mutex_lock( &QueueMutex );
	}
	
	// Unlock the queue
	void unlockQueue ()
	{
		pthread_mutex_unlock( &QueueMutex );
	}
#endif		


#endif

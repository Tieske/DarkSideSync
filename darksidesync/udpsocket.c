#ifndef dss_udpsocket_h
#define dss_udpsocket_h

#include <lua.h>
#include <lauxlib.h>
#include "udpsocket.h"
//#include "darksidesync.h"
#define DSS_TARGET "localhost"

#ifdef WIN32
//#include <windows.h>
//#include <stdio.h>
//static HANDLE SocketMutex;
//static HANDLE QueueMutex;

#else  // Unix

#include <netdb.h>
#include <unistd.h>
#include <string.h>
static volatile struct sockaddr_in receiver_addr;
static volatile int udpsock = -1;

#endif





/*
** ===============================================================
** Socket functions
** ===============================================================
*/

#ifdef WIN32

	void destroySocket ()
	{
	}

	// Creates and initializes the socket
	// @returns; 0 on failure, 1 on success
	int createSocket (int port)
	{
		return 0;	// TODO: implement win version
	}

	// Sends a UDP packet
	// @returns; 0 on failure, 1 on success
	int sendPacket (char *pData)
	{
		return 0;	// TODO: implement win version
	}

#else   // Unix

	void destroySocket ()
	{
		// Close and destroy socket
		if (udpsock > 0)
		{
			close(udpsock);
			udpsock = -1;
		}
	}


	// Creates and initializes the socket
	// @returns; 0 on failure, 1 on success
	int createSocket (int port)
	{
		struct hostent *hp;
		
		if (port != 0)
		{
			// Create and return UDP socket for port number
			udpsock = socket(AF_INET, SOCK_DGRAM, 0);
			if (udpsock < 0) return 0;	// report failure

			// lookup 'localhost' 
			receiver_addr.sin_family = AF_INET;
			hp = gethostbyname(DSS_TARGET);
			if (hp == 0)
			{
				// unkown host
				destroySocket();
				return 0;	// report failure
			}

			// Set server and port
			bcopy((char *)hp->h_addr, 
				(char *)&receiver_addr.sin_addr,
				hp->h_length);
			receiver_addr.sin_port = htons(port);
			
		}
		return 1;
	}

	// Sends a UDP packet
	// @returns; 0 on failure, 1 on success
	int sendPacket (char *pData)
	{
		if (udpsock > 0)
		{
			int n;
			// Send string as UDP packet
			n = sendto(udpsock, &pData,
				strlen(pData), 0, (struct sockaddr*)&receiver_addr,
				sizeof(receiver_addr));
			if (n < 0) return 0;	// report failure
			
			return 1;	// report success
		}
		return 0;	// report failure (there was no socket)
	}

#endif

#endif

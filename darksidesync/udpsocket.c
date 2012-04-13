#ifndef dss_udpsocket_h
#define dss_udpsocket_h

#include <lua.h>
#include <lauxlib.h>
#include "udpsocket.h"
//#include "darksidesync.h"
#define DSS_TARGET "localhost"


#ifdef WIN32

#include <winsock.h>
static volatile SOCKET udpsock = INVALID_SOCKET;	
static WSADATA w;
static struct hostent *hp;

#else  // Unix

#include <netdb.h>
#include <unistd.h>
#include <string.h>
static volatile int udpsock = INVALID_SOCKET;	// TODO: symbol instead of constant??

#endif

static volatile struct sockaddr_in receiver_addr;




/*
** ===============================================================
** Socket functions
** ===============================================================
*/

#ifdef WIN32

	void destroySocket ()
	{
		if (udpsock != INVALID_SOCKET)
		{
			closesocket(udpsock);
			WSACleanup();
			udpsock = INVALID_SOCKET;
		}
	}

	// Creates and initializes the socket
	// @returns; 0 on failure, 1 on success
	int createSocket (int port)
	{
		if (port != 0)
		{
			/* Open windows connection */
			if (WSAStartup(0x0101, &w) != 0) 
			{
				return 0;	//failed to initialize
			}

			/* Open a datagram socket */
			udpsock = socket(AF_INET, SOCK_DGRAM, 0);
			if (udpsock == INVALID_SOCKET)
			{
				WSACleanup();
				return 0;	//failed to create socket
			}

			/* Clear out server struct */
			memset((void *)&receiver_addr, '\0', sizeof(struct sockaddr_in));

			/* Set family and port */
			receiver_addr.sin_family = AF_INET;
			receiver_addr.sin_port = htons(port);

			/* Get localhost address */
			hp = gethostbyname(DSS_TARGET);

			/* Check for NULL pointer */
			if (hp == NULL)
			{
				closesocket(udpsock);
				WSACleanup();
				udpsock = INVALID_SOCKET;
				return 0;		// failed to resolve localhost name
			}

			/* Set target address */
			receiver_addr.sin_addr.S_un.S_un_b.s_b1 = hp->h_addr_list[0][0];
			receiver_addr.sin_addr.S_un.S_un_b.s_b2 = hp->h_addr_list[0][1];
			receiver_addr.sin_addr.S_un.S_un_b.s_b3 = hp->h_addr_list[0][2];
			receiver_addr.sin_addr.S_un.S_un_b.s_b4 = hp->h_addr_list[0][3];
		}
		return 1;	
	}

	// Sends a UDP packet
	// @returns; 0 on failure, 1 on success
	int sendPacket (char *pData)
	{
		if (udpsock > 0)
		{
			/* Tranmsit data to get time */
			//server_length = sizeof(struct sockaddr_in);
			if (sendto(udpsock, pData, (int)strlen(pData) + 1, 0, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr_in)) == -1)
			{
				closesocket(udpsock);
				WSACleanup();
				udpsock = INVALID_SOCKET;
				return 0;	// report failure to send
			}
			return 1;	// report success
		}
		return 0;	// report failure (there was no socket)
	}

#else   // Unix

	void destroySocket ()
	{
		// Close and destroy socket
		if (udpsock != INVALID_SOCKET)
		{
			close(udpsock);
			udpsock = INVALID_SOCKET;
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
			if (udpsock < 0) {
				udpsock = INVALID_SOCKET;
				return 0;	// report failure
			}

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
		if (udpsock != INVALID_SOCKET)
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

#ifndef dss_udpsocket_c
#define dss_udpsocket_c

#include <lua.h>
#include <lauxlib.h>
#include "udpsocket.h"

#define DSS_TARGET "localhost"

#ifdef WIN32
	static WSADATA w;
#endif


/*
** ===============================================================
** Socket functions
** ===============================================================
*/
// Init network
// return 1 upon success
int udpsocket_networkInit()
{
	int result = 1;
	#ifdef WIN32
		if (WSAStartup(0x0101, &w) != 0) result = 0;
	#endif
	return result;
}

// Shutdown network
void udpsocket_networkStop()
{
	#ifdef WIN32
		WSACleanup();
	#endif
}

// Close socket
void udpsocket_close(udpsocket_t s)
{
	if (s.udpsock != INVALID_SOCKET)
	{
		#ifdef WIN32
			closesocket(s.udpsock);
		#else
			close(s.udpsock);
		#endif
		s.udpsock = INVALID_SOCKET;
	}
}


// Create a new socket
// return socket struct, failed if member; udpsock == INVALID_SOCKET
// Port == 0 always fails
udpsocket_t udpsocket_new(int port)
{
	udpsocket_t s;
	struct hostent *hp;
	s.udpsock = INVALID_SOCKET;

	if (port != 0)
	{
		#ifdef WIN32
			/* Open a datagram socket */
			s.udpsock = socket(AF_INET, SOCK_DGRAM, 0);
			if (s.udpsock == INVALID_SOCKET)
			{
				return s;	//failed to create socket
			}

			/* Clear out server struct */
			memset((void *)&(s.receiver_addr), '\0', sizeof(struct sockaddr_in));

			/* Set family and port */
			s.receiver_addr.sin_family = AF_INET;
			s.receiver_addr.sin_port = htons(port);

			/* Get localhost address */
			hp = gethostbyname(DSS_TARGET);

			/* Check for NULL pointer */
			if (hp == NULL)
			{
				closesocket(s.udpsock);
				s.udpsock = INVALID_SOCKET;
				return s;		// failed to resolve localhost name
			}

			/* Set target address */
			s.receiver_addr.sin_addr.S_un.S_un_b.s_b1 = hp->h_addr_list[0][0];
			s.receiver_addr.sin_addr.S_un.S_un_b.s_b2 = hp->h_addr_list[0][1];
			s.receiver_addr.sin_addr.S_un.S_un_b.s_b3 = hp->h_addr_list[0][2];
			s.receiver_addr.sin_addr.S_un.S_un_b.s_b4 = hp->h_addr_list[0][3];

		#else

			// Create and return UDP socket for port number
			s.udpsock = socket(AF_INET, SOCK_DGRAM, 0);
			if (s.udpsock < 0) {
				s.udpsock = INVALID_SOCKET;
				return s;	// report failure
			}

			// lookup 'localhost' 
			s.receiver_addr.sin_family = AF_INET;
			hp = gethostbyname(DSS_TARGET);
			if (hp == 0)
			{
				// unkown host
				close(s.udpsock);
				s.udpsock = INVALID_SOCKET;
				return s;	// report failure
			}

			// Set server and port
			bcopy((char *)hp->h_addr, 
				(char *)&s.receiver_addr.sin_addr,
				hp->h_length);
			s.receiver_addr.sin_port = htons(port);

		#endif
	}

	return s;
}

// Sends packet, failure reported as 0, failure closes socket
int udpsocket_send(udpsocket_t s, char *pData)
{
	if (s.udpsock != INVALID_SOCKET)
	{
		//TODO: remove platform stuff; why the +1 for length at WIN32 and not the other? so far for copy-paste examples :-/
		#ifdef WIN32
			/* Tranmsit data */
			if (sendto(s.udpsock, pData, (int)strlen(pData) + 1, 0, (struct sockaddr *)&s.receiver_addr, sizeof(struct sockaddr_in)) == -1)
			{
				closesocket(s.udpsock);
				s.udpsock = INVALID_SOCKET;
				return 0;	// report failure to send
			}
		#else
			int n;
			// Send string as UDP packet
			n = sendto(s.udpsock, &pData,
				strlen(pData), 0, (struct sockaddr*)&s.receiver_addr,
				sizeof(s.receiver_addr));
			if (n < 0) 
			{
				close(s.udpsock);
				s.udpsock = INVALID_SOCKET;
				return 0;	// report failure
			}
		#endif
		return 1;	// report success
	}
	return 0;	// report failure (there was no socket)
}

#endif

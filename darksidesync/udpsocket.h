#ifndef dss_udpsocket_h
#define dss_udpsocket_h

#ifdef WIN32
	#include <winsock.h>
#else
	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1		// Define value for no valid socket
	#endif
	#include <netdb.h>
	#include <unistd.h>
	#include <string.h>
#endif

// socket structure
typedef struct udpsocket {
	#ifdef WIN32
		SOCKET udpsock;	
	#else  // Unix
		int udpsock;	
	#endif
	struct sockaddr_in receiver_addr;
} udpsocket_t;

// Init / teardown network
int udpsocket_networkInit();
void udpsocket_networkStop();

// Socket operations
udpsocket_t udpsocket_new(int port);
void udpsocket_close(udpsocket_t s);
int udpsocket_send(udpsocket_t s, char *pData);

#endif  /* dss_udpsocket_h */


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
typedef struct DSS_socket {
	#ifdef WIN32
		SOCKET udpsock;	
	#else  // Unix
		int udpsock;	
	#endif
	struct hostent *hp;
	struct sockaddr_in receiver_addr;
} DSS_socket_t;

// Init / teardown network
int DSS_networkInit();
void DSS_networkStop();

// Socket operations
DSS_socket_t DSS_socketNew(int port);
void DSS_socketClose(DSS_socket_t s);
int DSS_socketSend(DSS_socket_t s, char *pData);

#endif  /* dss_udpsocket_h */

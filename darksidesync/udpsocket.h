
#ifndef dss_udpsocket_h
#define dss_udpsocket_h

#ifndef WIN32
	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1		// Define value for no valid socket
	#endif
#endif

// Init / teardown network
int DSS_networkInit();
void DSS_networkStop()

// Socket operations
DSS_socket_t DSS_socketNew(int port)
void DSS_socketClose(DSS_socket_t s)
int DSS_socketSend(DSS_socket_t s, char *pData)

#endif  /* dss_udpsocket_h */

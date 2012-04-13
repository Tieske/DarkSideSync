
#ifndef dss_udpsocket_h
#define dss_udpsocket_h

int  createSocket (int port);
void destroySocket ();
int  sendPacket (char* pData);

#ifndef WIN32
	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1		// Define value for no valid socket
	#endif
#endif

#endif  /* dss_udpsocket_h */

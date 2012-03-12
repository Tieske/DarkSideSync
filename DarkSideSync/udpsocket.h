
#ifndef dss_udpsocket_h
#define dss_udpsocket_h

int  createSocket (int port);
void destroySocket ();
int  sendPacket (char* pData);

#endif

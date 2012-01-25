int UDPPort;
typedef struct DecodeStruct{
		int (*decode) (void *);
		void* data;
	} DecodeInfo;

void (*pDeliver) (DecodeInfo);

void deliver (DecodeInfo decodeInfo)
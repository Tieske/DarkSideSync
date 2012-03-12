
	int StartTimer ();
	int StopTimer ();
	typedef struct DecodeStruct{
		int (*decode) (void *);
		void* data;
	} DecodeInfo;

void (*pDeliver) (DecodeInfo);
	

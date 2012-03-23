#ifndef luaexit_h	// TODO: rename definition to a unique name
#define luaexit_h


// Macro to export the API
#ifndef EXPORT_API
	#ifdef WIN32
		#define EXPORT_API __declspec(dllexport)
	#else
		#define EXPORT_API extern
	#endif
#endif  

// add header implementation



#endif	

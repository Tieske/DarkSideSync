
#ifndef darksidesync_api_h
#define darksidesync_api_h

#include <lua.h>

// Setup version information
#define DSS_VERSION "0.1"

//////////////////////////////////////////////////////////////
// C side prototypes, implemented by background worker		//
//////////////////////////////////////////////////////////////

// The backgroundworker must provide this function. The function
// will get a pointer to previously delivered pData and is responsible
// for decoding this pData and take appropriate action.
// @arg1; the Lua state (or NULL if items are being cancelled)
// @arg2; the pData previously delivered. 
// @arg3;  the unique utility ID for which the call is being made (in case
// the utility has been 'required' in multiple parallel lua states)
typedef void (*DSS_decoder_1v0_t) (lua_State *L, void* pData, void* utilid);

// The backgroundworker must provide this function. A pointer
// to this method should be provided when calling the DSS_register function.
// When called, the backgroundworker should stop delivering and 
// unregister itself with DSS.
// @arg1;  the unique utility ID for which the call is being made (in case
// the utility has been 'required' in multiple parallel lua states)
// @arg2; pointer to the data provided when registering, see DSS_register_1v0_t
typedef void (*DSS_cancel_1v0_t) (void* utilid, void* pData);


//////////////////////////////////////////////////////////////
// C side prototypes, implemented by DSS					//
//////////////////////////////////////////////////////////////

// The backgroundworker can call this function (see
// DSS_REGISTRY_NAME for collecting it) to deliver data
// to the Lua state.
// @arg1; ID of utility delivering (see register() function)
// @arg2: pointer to a decoder function (see DSS_decoder_t above)
// @arg3; pointer to some piece of data.
// @returns; DSS_SUCCESS, DSS_ERR_INVALID_UTILID, DSS_ERR_UDP_SEND_FAILED, 
// DSS_ERR_OUT_OF_MEMORY, DSS_ERR_NOT_STARTED
// NOTE1: DSS_ERR_UDP_SEND_FAILED means that the data was still delivered to the
// queue, only the notification failed, for the other errors, it will not be
// queued.
// NOTE2: edgecase due to synchronization, when delivering while DSS is stopping
// DSS_ERR_INVALID_UTILID may be returned, even if cancel() was not called yet,
// so this should always be checked
typedef int (*DSS_deliver_1v0_t) (void* utilid, DSS_decoder_1v0_t pDecode, void* pData);

// Returns the data associated with the given utilid
// @arg1; ID of utility delivering (see register() function)
// @arg2; int pointer that will receive the error code DSS_ERR_INVALID_UTILID,
// or DSS_SUCCESS if no error (param may be NULL)
// Returns; pointer to the data provided when registering, see DSS_register_1v0_t,
// or NULL if an error occured (or the actual data was NULL).
typedef void* (*DSS_getdata_1v0_t) (void* utilid, int* errcode);

// Sets the data associated with the given utilid
// @arg1; ID of utility delivering (see register() function)
// @arg2; pointer to the data
// returns errorcode; DSS_SUCCESS, or DSS_ERR_INVALID_UTILID;
typedef int (*DSS_setdata_1v0_t) (void* utilid, void* pData);

// returns the utilid, for the combination of the LuaState and libid provided
// when handling a call form Lua, this enables access to the utilid, without
// having to explcitly manage utilid's across different LuaStates.
// @arg1; LuaState pointer, this identifies a unique LuaState
// @arg2; libid, generic pointer as an ID to a library (ID is shared across LuaStates)
// @arg3; int pointer that will receive the error code DSS_ERR_INVALID_UTILID,
// DSS_ERR_NOT_STARTED or DSS_SUCCESS if no error (param may be NULL)
// Returns: utilid, the ID that uniqueliy identifies the combination of a 
// LuaState and a background worker, or NULL upon failure (check errcode).
typedef void* (*DSS_getutilid_1v0_t) (lua_State *L, void* libid, int* errcode);

// The background worker should call this to register and get its ID
// @arg1; pointer to LuaState
// @arg2; ID for the library registering, can simply be;
//          static void* myLibID = &myLibID;	// pointer to itself
// @arg3; pointer to the background workers cancel() method
// @arg4; pointer to data specific to the background worker and the LuaState
// NOTE: it is up to the background worker to release any resources related to pData
// @arg5; int pointer that will receive the error code, or DSS_SUCCESS if no error (param may be NULL)
// @returns; unique ID (for the utility to use in other calls), or NULL and error
// DSS_ERR_NOT_STARTED, DSS_ERR_NO_CANCEL_PROVIDED, DSS_ERR_OUT_OF_MEMORY
typedef void* (*DSS_register_1v0_t) (lua_State *L, void* libid, DSS_cancel_1v0_t pCancel, void* pData, int* errcode);

// The background worker should call this to unregister itself on
// shutdown. Any items left in the queue will be cancelled.
// Note: After unregistering, the pData is no longer accessible. So the background
// worker should make sure to collect it before unregistering and (later on) 
// dispose of it properly.
// @arg1; the ID of the background worker to unregister
// @returns: DSS_SUCCESS, DSS_ERR_INVALID_UTILID
typedef int (*DSS_unregister_1v0_t) (void* utilid);


// Define global names for the Lua registry
#define DSS_REGISTRY_NAME "darksidesync"	// key to registry to where DSS will store its API's
#define DSS_VERSION_KEY "version"			// key to version info within DSS table
#define DSS_API_1v0_KEY "DSS api 1v0"		// key to struct with this API version, also version string in API struct

// Define structure to contain the API for version 1.0
typedef struct DSS_api_1v0_s *pDSS_api_1v0_t;
typedef struct DSS_api_1v0_s {
		const char* version;
		DSS_register_1v0_t reg;
		DSS_getutilid_1v0_t getutilid;
		DSS_deliver_1v0_t deliver;
		DSS_getdata_1v0_t getdata;
		DSS_setdata_1v0_t setdata;
		DSS_unregister_1v0_t unreg;
	} DSS_api_1v0_t;


//////////////////////////////////////////////////////////////
// C side DSS return codes									//
//////////////////////////////////////////////////////////////
#define DSS_SUCCESS -1					// success
#define DSS_ERR_INVALID_UTILID -2		// provided ID does not exist/invalid
#define DSS_ERR_UDP_SEND_FAILED -3		// notification failed due to UDP/socket error
#define DSS_ERR_NOT_STARTED -4			// DSS hasn't been started, or was already stopping/stopped
#define DSS_ERR_NO_CANCEL_PROVIDED -5	// When registering the cancel method is required
#define DSS_ERR_OUT_OF_MEMORY -6		// memory allocation failed
#define DSS_ERR_INIT_MUTEX_FAILED -7	// initialization of mutex failed
#endif /* darksidesync_api_h */

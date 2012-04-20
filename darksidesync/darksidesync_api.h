
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
// @arg2; the pData previously delivered. NOTE: it is up to the background
// worker to release any resources related to pData.
// @arg3;  the unique utility ID for which the call is being made (in case
// the utility has been 'required' in multiple parallel lua states)
typedef void (*DSS_decoder_1v0_t) (lua_State *L, void* pData, void* utilid);

// The backgroundworker must provide this function. A pointer
// to this method should be provided when calling the DSS_register function.
// When called, the backgroundworker should stop delivering and 
// unregister itself with DSS.
// @arg1;  the unique utility ID for which the call is being made (in case
// the utility has been 'required' in multiple parallel lua states)
typedef void (*DSS_cancel_1v0_t) (void* utilid);


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
typedef int (*DSS_deliver_1v0_t) (int utilid, DSS_decoder_1v0_t pDecode, void* pData);

// The background worker should call this to register and get its ID
// @arg1; pointer to LuaState
// @arg2; pointer to the background workers cancel() method
// @returns; unique ID for the utility to use in other calls (>0), or
// DSS_ERR_NOT_STARTED, DSS_ERR_NO_CANCEL_PROVIDED, DSS_ERR_OUT_OF_MEMORY
typedef int (*DSS_register_1v0_t) (lua_State *L, DSS_cancel_1v0_t pCancel);

// The background worker should call this to unregister itself on
// shutdown. Any items left in the queue will be cancelled.
// @arg1; the ID of the background worker to unregister
// @returns: DSS_SUCCESS, DSS_ERR_INVALID_UTILID
typedef int (*DSS_unregister_1v0_t) (int utilid);


// Define global names for the Lua registry
#define DSS_REGISTRY_NAME "darksidesync"	// key to registry to where DSS will store its API's
#define DSS_VERSION_KEY "version"			// key to version info within DSS table
#define DSS_API_1v0_KEY "DSS api 1v0"		// key to struct with this API version, also version string in API struct

// Define structure to contain the API for version 1.0
typedef struct DSS_api_1v0_s *pDSS_api_1v0_t;
typedef struct DSS_api_1v0_s {
		const char* version;
		DSS_register_1v0_t reg;
		DSS_deliver_1v0_t deliver;
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

#endif /* darksidesync_api_h */

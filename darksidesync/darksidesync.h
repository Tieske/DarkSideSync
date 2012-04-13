
#ifndef darksidesync_h
#define darksidesync_h

#include <lua.h>

// Define global name for the Lua registry
// The background worker can collect a LightUserData
// from the registry using this name.
// The pointer contained therein is of type
// DSS_deliver_t (see DSS_deliver_t)
#define DSS_REGISTRY_NAME "darksidesync"

//////////////////////////////////////////////////////////////
// C side prototypes, implemented by background worker		//
//////////////////////////////////////////////////////////////

// The backgroundworker must provide this function. The function
// will get a pointer to previously delivered pData and is responsible
// for decoding this pData and take appropriate action.
// @arg1; the Lua state (or NULL if items are being cancelled)
// @arg2; the pData previously delivered. NOTE: it is up to the background
// worker to release any resources related to pData.
typedef void (*DSS_decoder_t) (lua_State *L, void* pData);

// The backgroundworker must provide this function. A pointer
// to this method should be provided when calling the DSS_register function.
// When called, the backgroundworker should stop delivering and 
// unregister itself with DSS.
typedef void (*DSS_cancel_t) ();


//////////////////////////////////////////////////////////////
// C side prototypes, implemented by DSS					//
//////////////////////////////////////////////////////////////

// The backgroundworker can call this function (see
// DSS_REGISTRY_NAME for collecting it) to deliver data
// to the Lua state.
// @arg1; ID of utility delivering (see register() function)
// @arg2: pointer to a decoder function (see DSS_decoder_t above)
// @arg3; pointer to some piece of data.
// @returns; DSS_SUCCESS, DSS_ERR_INVALID_UTILID, DSS_ERR_UDP_SEND_FAILED, DSS_ERR_OUT_OF_MEMORY
// NOTE: edgecase due to synchronization, when delivering while DSS is stopping
// DSS_ERR_INVALID_UTILID may be returned, even if cancel() was not called yet,
// so this should always be checked
typedef int (*DSS_deliver_t) (int utilid, DSS_decoder_t pDecode, void* pData);

// The background worker should call this to register and get its ID
// @arg1; pointer to the background workers cancel() method
// @returns; unique ID for the utility to use in other calls (>0), or
// DSS_ERR_NOT_STARTED, DSS_ERR_NO_CANCEL_PROVIDED, DSS_ERR_OUT_OF_MEMORY
typedef int (*DSS_register_t) (DSS_cancel_t pCancel);

// The background worker should call this to unregister itself on
// shutdown. Any items left in the queue will be cancelled.
// @arg1; the ID of the background worker to unregister
// @returns: DSS_SUCCESS, DSS_ERR_INVALID_UTILID
typedef int (*DSS_unregister_t) (int utilid);

//////////////////////////////////////////////////////////////
// C side DSS return codes									//
//////////////////////////////////////////////////////////////
#define DSS_SUCCESS -1					// success
#define DSS_ERR_INVALID_UTILID -2		// provided ID does not exist/invalid
#define DSS_ERR_UDP_SEND_FAILED -3		// notification failed due to UDP/socket error
#define DSS_ERR_NOT_STARTED -4			// DSS hasn't been started, or was already stopping/stopped
#define DSS_ERR_NO_CANCEL_PROVIDED -5	// When registering the cancel method is required
#define DSS_ERR_OUT_OF_MEMORY -6		// memory allocation failed
#endif /* darksidesync_h */

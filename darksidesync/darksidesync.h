
#ifndef darksidesync_h
#define darksidesync_h

#include <lua.h>

// Define global name for the Lua registry
// The background worker can collect a LightUserData
// from the registry using this name.
// The pointer contained therein is of type
// DSS_deliver_t (see DSS_deliver_t)
#define DSS_REGISTRY_NAME "darksidesync"

// C side prototypes

// The backgroundworker must provide this function. The function
// will get a pointer to previously delivered pData and is responsible
// for decoding this pData into valid arguments on the Lua stack.
// @arg1; the Lua state on which stack to put the decoded pData
// @arg2; the pData previously delivered. NOTE: after the call returns
// the memory allocated for pData will be released and is no longer
// safe to access.
// @returns; similar to c function for Lua it should return the number
// of arguments it decoded on to the Lua stack.
typedef void (*DSS_decoder_t) (lua_State *L, void* pData);

// The backgroundworker can call this function (see
// DSS_REGISTRY_NAME for collecting it) to deliver data
// to the Lua state.
// @arg1; ID of utility delivering (see register() function)
// @arg2: pointer to a decoder function (see DSS_decoder_t below)
// @arg3; pointer to some piece of data. NOTE: once delivered
// DSS will take ownership of the memory allocated for this item.
// @returns; 0 on error sending UDP packet, 1 otherwise
typedef int (*DSS_deliver_t) (long utilid, DSS_decoder_t pDecode, void* pData);

#endif /* darksidesync_h */

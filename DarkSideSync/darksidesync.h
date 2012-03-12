
#ifndef darksidesync_h
#define darksidesync_h

// Define global name for the Lua registry
#define DSS_REGISTRY_NAME "darksidesync"

// C side prototypes
typedef void (*DSS_decoder_t) (lua_State *L, void* pData); 	// Decoder function definition
typedef int (*DSS_deliver_t) (DSS_decoder_t pDecode, void* pData);	// Deliver function definition

#endif

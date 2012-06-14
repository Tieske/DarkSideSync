#include <stdlib.h>
#include <lauxlib.h>
#include <string.h>
#include "darksidesync_api.h"

// static pointer to itself, uniquely identifies this library
static void* DSS_LibID = &DSS_LibID;
// Static pointer to the DSS api, will be set by the initialize function below
static pDSS_api_1v0_t DSSapi = NULL;

// will return the api struct (pointer) for the api version 1v0
// in case of errors it will provide a proper error message and call 
// luaL_error. In case of an error the call will not return.
static void DSS_initialize(lua_State *L, DSS_cancel_1v0_t pCancel)
{
	int errcode;

	// Collect the table with the global DSS data from the register
	lua_getfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);
	if (lua_istable(L, -1) == 0)
	{
		// following call does not return
		luaL_error(L, "No DSS registry table found, make sure to require 'darksidesync' first.");
	}
	// Now get the specified API structure
	lua_getfield(L, -1, DSS_API_1v0_KEY);
	if (lua_islightuserdata (L, -1) == 0)
	{
		// the API struct wasn't found
		char str[150]; // appr 40 chars for version string, should suffice
		lua_getfield(L, -2, DSS_VERSION_KEY);
		if (lua_isstring(L, -1))
		{
			sprintf(str, "Loaded DSS version '%s' does not support API version '%s'. Make sure to use the proper DSS version.", lua_tostring(L, -1), DSS_API_1v0_KEY);
		}
		else
		{
			sprintf(str, "Loaded DSS version is unknown, it does not support API version '%s'. Make sure to use the proper DSS version.", DSS_API_1v0_KEY);
		}
		// following call does not return
		luaL_error(L, str);
	}
	DSSapi = lua_touserdata(L, -1);
	lua_pop(L,2); // pop apistruct and DSS global table

	// Now register ourselves
	// pData = NULL, because on errors the initialize function will not return
	// and pData may leak resources, so set the pData after initializing.
	DSSapi->reg(L, DSS_LibID, pCancel, NULL, &errcode);
	if (errcode != DSS_SUCCESS)
	{
		DSSapi = NULL;
		// The error calls below will not return
		switch (errcode) {
			case DSS_ERR_NOT_STARTED: luaL_error(L, "DSS was not started, or already stopped again.");
			case DSS_ERR_NO_CANCEL_PROVIDED: luaL_error(L, "No proper cancel method was provided when initializing DSS.");
			case DSS_ERR_OUT_OF_MEMORY: luaL_error(L, "Memory allocation error while initializing DSS");
			default: luaL_error(L, "An unknown error occured while initializing DSS.");
		}
	}

	return;
}

// at shutdown, this will safely unregister the library
// use lua_State param if called from userdata __gc method
// use utilid param if called from the DSS cancel() method
// one param must be provided, lua_State has highest presedence
static void DSS_shutdown(lua_State *L, void* utilid)
{
	if (DSSapi != NULL) 
	{
		// If we got a Lua state, go lookup our utilid
		if (L != NULL) utilid = DSSapi->getutilid(L, DSS_LibID, NULL);
		// Unregister
		if (utilid != NULL) DSSapi->unreg(utilid);
		DSSapi = NULL;
	}
}

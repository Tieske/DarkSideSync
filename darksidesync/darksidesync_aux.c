#include <stdlib.h>
#include <lauxlib.h>
#include <string.h>
#include "darksidesync.h"


// will return the api struct (pointer) for the specified api version
// in case of errors it will provide a proper error message and call 
// luaL_error. In case of an error the call will not return.
void *DSS_initialize(lua_State *L, char* api_key)
{
	void *apistruct;

	if (lua_checkstack (L, 3) == 0)
	{
		// following call does not return
		luaL_error(L, "Out of Lua stack space.");
	}

	// Collect the table with the global DSS data from the register
	lua_getfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);
	if (lua_istable(L, -1) == 0)
	{
		// following call does not return
		luaL_error(L, "No DSS registry table found, make sure to require 'darksidesync' first.");
	}
	// Now get the specified API structure
	lua_getfield(L, -1, api_key);
	if (lua_islightuserdata (L, -1) == 0)
	{
		// the API struct wasn't found
		char str[150]; // appr 40 chars for version string, should suffice
		strcpy (str, "Loaded DSS version '");
		lua_getfield(L, -2, DSS_VERSION_KEY);
		if (lua_isstring(L, -1))
		{
			strcat(str, lua_tostring(L, -1));
		}
		else
		{
			strcat(str, "unknown");
		}
		strcat (str, "' does not support API version '");
		strcat (str, api_key);
		strcat (str, "'. Make sure to use the proper DSS version.");
		// following call does not return
		luaL_error(L, str);
	}
	apistruct = lua_touserdata(L, -1);
	lua_pop(L,2); // pop apistruct and DSS global table

	return apistruct;
}

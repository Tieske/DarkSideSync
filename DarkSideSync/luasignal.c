#include <lua.h>
#include <lauxlib.h>
#include <signal.h>
#include "darksidesync.h"


// Name to anchor callback in the Lua register
#define LUASIGNAL_CALLBACK_NAME "LuaSignalCallBack"

DSS_deliver_t DeliverFunction = NULL;

/*
** ===============================================================
** Signal handling code
** ===============================================================
*/

	// Decodes data and puts it on the Lua stack
	// pData is always NULL in this case, because we only handle
	// SIGTERM signal, so push constant string
	void signalDecoder (lua_State *L, void *pData)
	{
		lua_pushstring(L, "SIGTERM");
		return;
	}
	
	void signalHandler(int sigNum)
	{
		signal(SIGTERM, SIG_IGN);	// Temporarily ignore signals

		// deliver signal to DarkSideSync, no data included
		DeliverFunction (signalDecoder, NULL); // TODO: this is probably not safe in a signal handler!!!!!
		
		signal(SIGTERM, signalHandler); // Set handler again
	}

/*
** ===============================================================
** Lua API
** ===============================================================
*/
	// Lua function to start the library
	// Params; 1, function to use as callback for the signal
	int L_start(lua_State *L)
	{
		// anchor provided callback function in register
		if (lua_gettop(L) >= 1 && lua_isfunction(L,1))
		{
			lua_settop(L,1);
			lua_setfield(L, LUA_REGISTRYINDEX, LUASIGNAL_CALLBACK_NAME);
		}
		else
		{
			// not a function, error
			lua_settop(L,0);
			lua_pushnil(L);
			lua_pushstring(L, "Expected single argument of type function, to be used as callback");
			return 1;
		}
		// Collect the pointer to the deliver function from the register
		lua_settop(L,0);
		lua_getfield(L, LUA_REGISTRYINDEX, DSS_REGISTRY_NAME);
		if (NULL == (DeliverFunction = lua_touserdata(L, 1)))
		{
			// no deliver function, error
			lua_settop(L,0);
			lua_pushnil(L);
			lua_pushstring(L, "No delivery possibility found, make sure to require 'darksidesync' first");
			return 1;
		}
		signal(SIGTERM, signalHandler);
		lua_settop(L,0);
		return 1;
	};

	// Lua function to stop the library and clear the callback
	int L_stop (lua_State *L)
	{
		signal(SIGTERM, SIG_DFL);	// set to default handler
		// Clear callback function from register
		lua_settop(L,0);
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, LUASIGNAL_CALLBACK_NAME);
		return 1;
	};

/*
** ===============================================================
** Library initialization
** ===============================================================
*/
	static const struct luaL_Reg LuaSignal[] = {
		{"start",L_start},
		{"stop",L_stop},
		{NULL,NULL}
	};

	int luaopen_luasignal(lua_State *L){
		luaL_register(L,"luasignal",LuaSignal);
		return 1;
	};



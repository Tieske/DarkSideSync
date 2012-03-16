#include <lua.h>
#include <lauxlib.h>
#include <signal.h>
#include "darksidesync.h"


static volatile int CallbackReference = LUA_NOREF;
static DSS_deliver_t DeliverFunction = NULL;

/*
** ===============================================================
** Signal handling code
** ===============================================================
*/

	// Decodes data and puts it on the Lua stack
	// pData is always NULL in this case, because we only handle
	// SIGTERM signal, so push constant string
	// @returns; as with Lua function, return number of args on the stack to return
	int signalDecoder (lua_State *L, void *pData)
	{
		lua_settop(L, 0);
		if (CallbackReference == LUA_NOREF)
		{
			lua_pushnil(L);
			lua_pushstring(L, "luasignal; No callback function set");
			return 2;
		}
		else
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, CallbackReference);
			lua_pushstring(L, "SIGTERM");
			return 2;
		}
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
			CallbackReference = luaL_ref(L, LUA_REGISTRYINDEX);
		}
		else
		{
			// not a function, error
			lua_settop(L,0);
			lua_pushnil(L);
			lua_pushstring(L, "Expected single argument of type function, to be used as callback");
			return 2;
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
			return 2;
		}
		signal(SIGTERM, signalHandler);
		lua_settop(L,0);
		lua_pushinteger(L, 1);	// report success
		return 1;
	};

	// Lua function to stop the library and clear the callback
	int L_stop (lua_State *L)
	{
		signal(SIGTERM, SIG_DFL);	// set to default handler
		// Clear callback function from register
		luaL_unref(L, LUA_REGISTRYINDEX, CallbackReference);
		CallbackReference = LUA_NOREF;
		// set results
		lua_settop(L,0);
		lua_pushinteger(L, 1);	// report success
		return 0;
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



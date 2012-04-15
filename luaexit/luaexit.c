#include <lua.h>
#include <lauxlib.h>
#include <signal.h>
#include "luaexit.h"
#include "darksidesync.h"

#include "darksidesync_aux.c"

static volatile int CallbackReference = LUA_NOREF;
static pDSS_api_1v0_t DSS_API = NULL;
static int DSSutilid;

// TODO: for windows implement the following;
// SetConsoleCtrlHandler http://msdn.microsoft.com/en-us/library/windows/desktop/ms686016(v=vs.85).aspx
// RegisterServiceCtrlHandler http://msdn.microsoft.com/en-us/library/windows/desktop/ms685054(v=vs.85).aspx

/*
** ===============================================================
** Signal handling code
** ===============================================================
*/
	void DSScancel()
	{
		// todo: implement
	}

	// Decodes data and puts it on the Lua stack
	// pData is always NULL in this case, because we only handle
	// SIGTERM & SIGINT signal, so push constant string
	// @returns; as with Lua function, return number of args on the stack to return
	int signalDecoder (lua_State *L, void *pData)
	{
		if (L == NULL)
		{
			// element is being cancelled, do nothing

		}
		lua_settop(L, 0);
		if (CallbackReference == LUA_NOREF)
		{
			lua_pushnil(L);
			lua_pushstring(L, "luasignal; No callback function set");
			return 2;
		}
		else
		{
			// TODO: execute callback from here, no more Lua side code
			lua_rawgeti(L, LUA_REGISTRYINDEX, CallbackReference);
			lua_pushstring(L, "SIGTERM");  // TODO: update this, its more than SIGTERM
			return 2;
		}
	}
	
	void signalHandler(int sigNum)
	{
		signal(sigNum, SIG_IGN);	// Temporarily ignore signals

		// deliver signal to DarkSideSync, no data included
		(*DSS_API).deliver(DSSutilid, &signalDecoder, NULL); // TODO: this is probably not safe in a signal handler!!!!!
		
		signal(sigNum, signalHandler); // Set handler again
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

		// install signal handlers
		signal(SIGTERM, signalHandler);
		signal(SIGINT, signalHandler);
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
	static const struct luaL_Reg LuaExit[] = {
		{"start",L_start},
		{"stop",L_stop},
		{NULL,NULL}
	};

EXPORT_API	int luaopen_luaexit(lua_State *L){

		// First initialize the DSS client structure
		DSS_API = DSS_initialize(L, DSS_API_1v0_KEY);	// get API struct
		DSSutilid = (*DSS_API).reg(&DSScancel);			// register myself

		luaL_register(L,"luaexit",LuaExit);

		return 1;
	};


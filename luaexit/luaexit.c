#include <lua.h>
#include <lauxlib.h>
#include <signal.h>
#include "luaexit.h"
#include "darksidesync.h"
#include "darksidesync_aux.h"

static volatile int CallbackReference = LUA_NOREF;
static void* DSSutilid;

// TODO: for windows implement the following;
// SetConsoleCtrlHandler http://msdn.microsoft.com/en-us/library/windows/desktop/ms686016(v=vs.85).aspx
// RegisterServiceCtrlHandler http://msdn.microsoft.com/en-us/library/windows/desktop/ms685054(v=vs.85).aspx

// forward definitions
int L_stop (lua_State *L);

/*
** ===============================================================
** Signal handling code
** ===============================================================
*/

	// GC procedure to cleanup stuff

	static int LuaExit_exit(lua_State *L)
	{
#ifdef _DEBUG
OutputDebugStringA("LuaExit: unloading started...\n");
#endif

		L_stop(L);
		DSS_shutdown(L, NULL);
		return 0;

#ifdef _DEBUG
OutputDebugStringA("LuaExit: unloading completed\n");
#endif
	}

	void DSScancel(void* utilid)
	{
		DSS_shutdown(NULL, utilid);
	}

	// Decodes data and puts it on the Lua stack
	// pData is always NULL in this case, because we only handle
	// SIGTERM & SIGINT signal, so push constant string
	// @returns; as with Lua function, return number of args on the stack to return
	int signalDecoder (lua_State *L, void *pData, void *utilid)
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
		DSS_decoder_1v0_t DecodeFunc;
		signal(sigNum, SIG_IGN);	// Temporarily ignore signals

		DecodeFunc = &signalDecoder;
		// deliver signal to DarkSideSync, no data included
		DSSapi->deliver(DSSutilid, DecodeFunc, NULL, NULL); // TODO: this is probably not safe in a signal handler!!!!!
		
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

		DSS_cancel_1v0_t CancelFunc = &DSScancel;

#ifdef _DEBUG
OutputDebugStringA("LuaExit: LuaOpen started...\n");
#endif
		// Create userdata
		lua_newuserdata(L, sizeof(void*));
		// Create a metatable to GC the global data upon exit
		luaL_newmetatable(L, "LuaExit.gc");
		lua_pushstring(L, "__gc");
		lua_pushcfunction(L, &LuaExit_exit);
		lua_settable(L, -3);
		// now add a metatable to the userdata
		lua_setmetatable(L, -2);				// set it to the created userdata
		lua_setfield(L, LUA_REGISTRYINDEX, "LuaExit.userdata");	// anchor the userdata


		// First initialize the DSS client structure
		DSS_initialize(L, CancelFunc);		// initialize and get API struct
		//TODO: Warning: statement below gets the utilID as static! can't use lib in more than 1 luastate
		DSSutilid = DSSapi->getutilid(L, DSS_LibID, NULL);

		luaL_register(L,"luaexit",LuaExit);
#ifdef _DEBUG
OutputDebugStringA("LuaExit: LuaOpen completed\n");
#endif
		return 1;
	};


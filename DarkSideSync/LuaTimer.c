#include <lua.h>
#include <lauxlib.h>
#include "darksidesync.h"
//#include <WinUser.h>


	int StartTimer (lua_State *L)
	{
		lua_getfield(L,LUA_REGISTRYINDEX,"LuaCommunicationsDelivery");

	    //SetTimer(0,0,10000,TimeElapsed);
		pDeliver = lua_touserdata(L,1);
		return 0;
	};

	int StopTimer (lua_State *L)
	{
		return 0;
	};

	static const struct luaL_Reg LuaCommModule [] = {
		{"StartTimer",StartTimer},
		{"StopTimer",StopTimer},
		{NULL,NULL}
	};

	int luaopen_LuaCommModule(lua_State *L){
		luaL_register(L,"LuaCommModule",LuaCommModule);
		return 1;
	};

	void TimeElapsed( 
    long hwnd,        // handle to window for timer messages 
    int message,     // WM_TIMER message 
    int idTimer,     // timer identifier 
    int dwTime)     // current system time 
{ 

	}

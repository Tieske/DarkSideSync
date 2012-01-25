#include <lua.h>
#include <lauxlib.h>

	int StartTimer (lua_State *L)
	{
		return 0;
	}

	int StopTimer (lua_State *L)
	{
		return 0;
	}

	static const struct luaL_Reg LuaCommModule [] = {
		{"StartTimer",StartTimer},
		{"StopTimer",StopTimer},
		{NULL,NULL}
	};

	int luaopen_LuaCommModule(lua_State *L){
		luaL_register(L,"LuaCommModule",LuaCommModule);
		return 1;
	}

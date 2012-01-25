#include <lua.h>
#include <lauxlib.h>
#include "LuaCommunications.h"

	int Start(lua_State *L)
	{
		UDPPort = luaL_checkint(L,2);

		pDeliver = &deliver;
		
		lua_pushlightuserdata(L,pDeliver);

		lua_setfield(L,LUA_REGISTRYINDEX,"LuaCommunicationsDelivery");
	}
	int Poll(lua_State *L)
	{

	}

	static const struct luaL_Reg LuaCommunications[] = {
		{"Start",Start},
		{"Poll",Poll},
		{NULL,NULL}
	};

	int luaopen_LuaCommunications(lua_State *L){
		luaL_register(L,"LuaCommunications",LuaCommunications);
		return 1;
	}

	void deliver (DecodeInfo decodeInfo)
	{
		
	}
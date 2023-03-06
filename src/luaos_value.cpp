

#include "luaos.h"
#include "luaos_value.h"

/*******************************************************************************/

int lua_packany(lua_State* L, int index, std::string& out)
{
  lua_pushcfunction(L, pack_any);
  lua_pushvalue(L, index);
  int status = luaos_pcall(L, 1, 1);
  if (status != LUA_OK) {
    lua_pop(L, 1);  /* pop error from stack */
    return status;
  }
  size_t size = 0;
  const char* data = luaL_checklstring(L, -1, &size);
  out.assign(data, size);
  lua_pop(L, 1);
  return status;
}

/*******************************************************************************/

int lua_unpackany(lua_State* L, const std::string& data)
{
  if (data.empty()) {
    lua_pushnil(L);
    return 0;
  }
  lua_pushcfunction(L, unpack_any);
  lua_pushlstring(L, data.c_str(), data.size());
  int status = luaos_pcall(L, 1, 1);
  if (status != LUA_OK) {
    lua_pop(L, 1);  /* pop error from stack */
    lua_pushnil(L);
  }
  return status;
}

/*******************************************************************************/

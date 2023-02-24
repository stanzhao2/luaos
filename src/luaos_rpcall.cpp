

#include <string>
#include <map>
#include <vector>
#include <mutex>

#include "luaos_rpcall.h"

/*******************************************************************************/

typedef struct {
  int handler;
  io_handler ios;
} rpc_node;

static std::mutex  _mutex;
static std::map<std::string, rpc_node> _rpc_reged;
typedef std::vector<std::any> params_type;

/*******************************************************************************/

static void invoke(int index, const params_type& params, io_handler ios, int callback)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  int top = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  for (size_t i = 0; i < params.size(); i++) {
    lexpush_any(L, params[i]);
  }
  params_type result;
  auto status = luaos_pcall(L, (int)params.size(), LUA_MULTRET);
  result.push_back(status == LUA_OK ? true : false);

  int now = lua_gettop(L);
  for (int i = top + 1; i <= now; i++) {
    result.push_back(lexget_any(L, i));
  }
  ios->post([result, callback]()
  {
      lua_State* L = luaos_local.lua_state();
      stack_rollback rollback(L);

      lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
      for (size_t i = 0; i < result.size(); i++) {
        lexpush_any(L, result[i]);
      }
      luaL_unref(L, LUA_REGISTRYINDEX, callback);
      if (luaos_pcall(L, (int)result.size(), 0) != LUA_OK) {
        luaos_error("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
      }
    }
  );
}

/*******************************************************************************/

static void call(int index, const params_type& params, std::shared_ptr<params_type>& result, io_handler ios)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  int top = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  for (size_t i = 0; i < params.size(); i++) {
    lexpush_any(L, params[i]);
  }

  auto status = luaos_pcall(L, (int)params.size(), LUA_MULTRET);
  result->push_back(status == LUA_OK ? true : false);

  int now = lua_gettop(L);
  for (int i = top + 1; i <= now; i++) {
    result->push_back(lexget_any(L, i));
  }
  ios->stop();
}

/*******************************************************************************/

static int lua_rpcall_register(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  if (!lua_isfunction(L, 2)) {
    luaL_argerror(L, 2, "must be a function");
  }
  rpc_node node;
  node.handler = luaL_ref(L, LUA_REGISTRYINDEX);
  node.ios = luaos_local.lua_service();

  std::unique_lock<std::mutex> lock(_mutex);
  if (_rpc_reged.find(name) == _rpc_reged.end()) {
    _rpc_reged[name] = node;
    lua_pushboolean(L, 1);
  }
  else {
    luaL_unref(L, LUA_REGISTRYINDEX, node.handler);
    lua_pushboolean(L, 0);
  }
  return 1;
}

/*******************************************************************************/

static int lua_rpcall_cancel(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  std::unique_lock<std::mutex> lock(_mutex);

  auto iter = _rpc_reged.find(name);
  if (iter != _rpc_reged.end())
  {
    auto ios = luaos_local.lua_service();
    if (iter->second.ios->id() == ios->id())
    {
      luaL_unref(L, LUA_REGISTRYINDEX, iter->second.handler);
      _rpc_reged.erase(iter);
      lua_pushboolean(L, 1);
      return 1;
    }
  }
  lua_pushboolean(L, 0);
  return 1;
}

/*******************************************************************************/

static int lua_rpcall_call(lua_State* L)
{
  int argc = lua_gettop(L);
  const char* name = luaL_checkstring(L, 1);

  params_type params;
  for (int i = 2; i <= argc; i++) {
    params.push_back(lexget_any(L, i));
  }

  int index = 0;
  io_handler regios;
  {
    std::unique_lock<std::mutex> lock(_mutex);
    auto iter = _rpc_reged.find(name);
    if (iter != _rpc_reged.end())
    {
      index = iter->second.handler;
      regios = iter->second.ios;
    }
  }

  if (!index || !regios) {
    lua_pushboolean(L, 0);
    return 1;
  }

  auto wait = luaos_ionew();
  auto ios  = luaos_local.lua_service();
  std::shared_ptr<params_type> result = std::make_shared<params_type>();

  if (regios->id() == ios->id()) {
    call(index, params, result, wait);
  }
  else {
    regios->post(std::bind(&call, index, params, result, wait));
  }
  wait->run();
  for (size_t i = 0; i < result->size(); i++) {
    lexpush_any(L, (*result)[i]);
  }
  return (int)result->size();
}

/*******************************************************************************/

static int lua_rpcall_invoke(lua_State* L)
{
  int argc = lua_gettop(L);
  if (argc == 1 || !lua_isfunction(L, 2)) {
    return lua_rpcall_call(L);
  }

  params_type params;
  const char* name = luaL_checkstring(L, 1);
  for (int i = 3; i <= argc; i++) {
    params.push_back(lexget_any(L, i));
  }

  lua_pushvalue(L, 2);
  int callback = luaL_ref(L, LUA_REGISTRYINDEX);

  int index = 0;
  io_handler regios;
  {
    std::unique_lock<std::mutex> lock(_mutex);
    auto iter = _rpc_reged.find(name);
    if (iter != _rpc_reged.end())
    {
      index = iter->second.handler;
      regios = iter->second.ios;
    }
  }
  if (!index || !regios) {
    lua_pushboolean(L, 0);
    return 1;
  }
  auto ios = luaos_local.lua_service();
  regios->post(std::bind(&invoke, index, params, ios, callback));
  lua_pushboolean(L, 1);
  return 1;
}

/*******************************************************************************/

namespace rpcall
{
  void init_metatable(lua_State* L)
  {
    struct luaL_Reg methods[] = {
      { "register",      lua_rpcall_register  },
      { "call",          lua_rpcall_invoke    },
      { "cancel",        lua_rpcall_cancel    },
      { NULL,            NULL              },
    };
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setglobal(L, "rpcall");
  }
}

/*******************************************************************************/

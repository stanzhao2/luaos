

#include <string>
#include <map>
#include <vector>
#include <mutex>

#include "luaos.h"
#include "luaos_rpc.h"
#include "luaos_thread_local.h"

/*******************************************************************************/

typedef struct {
  int handler;
  reactor_type ios;
} rpc_node;

static std::mutex  _mutex;
static std::map<std::string, rpc_node> _rpc_reged;

typedef std::vector<std::any> params_type;

/*******************************************************************************/

namespace rpc
{
  void init_metatable(lua_State* L)
  {
    struct luaL_Reg rpc[] = {
      { "register",      lua_rpc_register  },
      { "call",          lua_rpc_invoke    },
      { "cancel",        lua_rpc_cancel    },
      { NULL,            NULL              },
    };
    lua_newtable(L);
    luaL_setfuncs(L, rpc, 0);
    lua_setglobal(L, "rpc");
  }
}

/*******************************************************************************/

static void invoke(int index, const params_type& params, reactor_type ios, int callback)
{
  lua_State* L = this_thread().lua_state();
  stack_checker checker(L);

  lua_pushcfunction(L, lua_pcall_error);
  int error_fn_index = lua_gettop(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  for (size_t i = 0; i < params.size(); i++) {
    lexpush_any(L, params[i]);
  }

  params_type result;
  auto ok = lua_pcall(L, (int)params.size(), LUA_MULTRET, error_fn_index);
  result.push_back(ok == LUA_OK ? true : false);

  int top = lua_gettop(L);
  for (int i = error_fn_index + 1; i <= top; i++) {
    result.push_back(lexget_any(L, i));
  }

  ios->post([result, callback]()
    {
      lua_State* L = this_thread().lua_state();
      stack_checker checker(L);

      lua_pushcfunction(L, lua_pcall_error);
      int error_fn_index = lua_gettop(L);

      lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
      for (size_t i = 0; i < result.size(); i++) {
        lexpush_any(L, result[i]);
      }
      lua_pcall(L, (int)result.size(), 0, error_fn_index);
      luaL_unref(L, LUA_REGISTRYINDEX, callback);
    }
  );
}

/*******************************************************************************/

static void call(int index, const params_type& params, std::shared_ptr<params_type>& result, reactor_type ios)
{
  lua_State* L = this_thread().lua_state();
  stack_checker checker(L);

  lua_pushcfunction(L, lua_pcall_error);
  int error_fn_index = lua_gettop(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  for (size_t i = 0; i < params.size(); i++) {
    lexpush_any(L, params[i]);
  }

  auto ok = lua_pcall(L, (int)params.size(), LUA_MULTRET, error_fn_index);
  result->push_back(ok == LUA_OK ? true : false);

  int top = lua_gettop(L);
  for (int i = error_fn_index + 1; i <= top; i++) {
    result->push_back(lexget_any(L, i));
  }
  ios->stop();
}

/*******************************************************************************/

LUALIB_API int lua_rpc_register(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  if (!lua_isfunction(L, 2)) {
    luaL_argerror(L, 2, "must be a function");
  }

  rpc_node node;
  node.handler = luaL_ref(L, LUA_REGISTRYINDEX);
  node.ios = this_thread().lua_reactor();

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

LUALIB_API int lua_rpc_cancel(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  std::unique_lock<std::mutex> lock(_mutex);

  auto iter = _rpc_reged.find(name);
  if (iter != _rpc_reged.end())
  {
    auto ios = this_thread().lua_reactor();
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

LUALIB_API int lua_rpc_call(lua_State* L)
{
  int argc = lua_gettop(L);
  const char* name = luaL_checkstring(L, 1);

  params_type params;
  for (int i = 2; i <= argc; i++) {
    params.push_back(lexget_any(L, i));
  }

  int index = 0;
  reactor_type regios;
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

  auto wait = reactor::create();
  auto ios  = this_thread().lua_reactor();
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

LUALIB_API int lua_rpc_invoke(lua_State* L)
{
  int argc = lua_gettop(L);
  if (argc == 1 || !lua_isfunction(L, 2)) {
    return lua_rpc_call(L);
  }

  const char* name = luaL_checkstring(L, 1);

  params_type params;
  for (int i = 3; i <= argc; i++) {
    params.push_back(lexget_any(L, i));
  }

  lua_pushvalue(L, 2);
  int callback = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1);

  int index = 0;
  reactor_type regios;
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

  auto ios = this_thread().lua_reactor();
  regios->post(std::bind(&invoke, index, params, ios, callback));
  lua_pushboolean(L, 1);
  return 1;
}

/*******************************************************************************/

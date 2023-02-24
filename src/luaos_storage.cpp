
/********************************************************************************
** 
** Copyright 2021-2022 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
********************************************************************************/

#include <map>
#include <mutex>
#include "luaos_storage.h"

static std::recursive_mutex _mutex;
static std::map<std::string, std::any> _storage;

/*******************************************************************************/

static int lua_storage_set(lua_State* L)
{
  std::any value;
  std::string key = luaL_checkstring(L, 1);
  luaL_checkany(L, 2);

  std::unique_lock<std::recursive_mutex> lock(_mutex);
  auto iter = _storage.find(key);
  if (iter != _storage.end()) {
    value = iter->second;
  }
  if (lua_isfunction(L, 2))
  {
    lua_pushvalue(L, 2);
    lexpush_any(L, value);
    if (luaos_pcall(L, 1, 1) != LUA_OK) {
      luaos_error("%s\n", lua_tostring(L, -1));
      lua_pop(L, 1); /* pop error from stack */
      return 0;
    }
  }
  _storage[key] = lexget_any(L, -1);
  lua_settop(L, 2);
  lexpush_any(L, value);
  return 1;
}

static int lua_storage_get(lua_State* L)
{
  std::string key = lexget_string(L, 1);
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  auto iter = _storage.find(key);
  if (iter == _storage.end()) {
    lua_pushnil(L);
    return 1;
  }
  lexpush_any(L, iter->second);
  return 1;
}

static int lua_storage_erase(lua_State* L)
{
  std::string key = lexget_string(L, 1);
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  std::any value;
  auto iter = _storage.find(key);
  if (iter != _storage.end())
  {
    value = iter->second;
    _storage.erase(iter);
  }
  lexpush_any(L, value);
  return 1;
}

static int lua_storage_clear(lua_State* L)
{
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  _storage.clear();
  return 0;
}

/*******************************************************************************/

namespace storage
{
  void init_metatable(lua_State* L)
  {
    struct luaL_Reg methods[] = {
      { "set",      lua_storage_set   },
      { "get",      lua_storage_get   },
      { "erase",    lua_storage_erase },
      { "clear",    lua_storage_clear },
      { NULL,       NULL },
    };
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setglobal(L, "global");
  }
}

/*******************************************************************************/

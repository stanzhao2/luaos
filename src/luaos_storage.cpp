
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
static std::map<std::string, lua_value> _storage;

/*******************************************************************************/

static int lua_storage_set(lua_State* L)
{
  lua_value old_value;
  std::string key = luaL_checkstring(L, 1);
  lua_value new_value = luaL_getvalue(L, 2);

  std::unique_lock<std::recursive_mutex> lock(_mutex);
  auto iter = _storage.find(key);
  if (iter != _storage.end()) {
    old_value = iter->second;
  }
  if (lua_isfunction(L, 3))
  {
    luaL_pushvalue(L, new_value);  /* push new value to stack */
    luaL_pushvalue(L, old_value);  /* push old value to stack */
    if (luaos_pcall(L, 2, 1) != LUA_OK) {
      luaos_error("%s\n", lua_tostring(L, -1));
      lua_pop(L, 1); /* pop error from stack */
      return 0;
    }
  }
  _storage[key] = luaL_getvalue(L, -1);
  luaL_pushvalue(L, old_value);
  return 1;
}

static int lua_storage_get(lua_State* L)
{
  std::string key = luaL_checkstring(L, 1);
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  auto iter = _storage.find(key);
  if (iter == _storage.end()) {
    lua_pushnil(L);
    return 1;
  }
  luaL_pushvalue(L, iter->second);
  return 1;
}

static int lua_storage_erase(lua_State* L)
{
  std::string key = luaL_checkstring(L, 1);
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  lua_value value;
  auto iter = _storage.find(key);
  if (iter != _storage.end())
  {
    value = iter->second;
    _storage.erase(iter);
  }
  luaL_pushvalue(L, value);
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

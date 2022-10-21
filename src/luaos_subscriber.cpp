
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

#include "luaos.h"
#include "luaos_subscriber.h"

typedef struct {
  int index;
  reactor_type ios;
} subscriber_item;

typedef std::vector<std::any> params_type;
typedef std::vector<subscriber_item> item_array;
typedef std::map<size_t, item_array> topic_array;

static std::mutex  _mutex;
static topic_array _topics;

/*******************************************************************************/

static void on_publish(int publisher, int index, const params_type& params)
{
  lua_State* L = this_thread().lua_state();
  stack_checker checker(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  lua_pushinteger(L, publisher);

  for (size_t i = 0; i < params.size(); i++) {
    lexpush_any(L, params[i]);
  }

  if (lua_pcall(L, (int)params.size() + 1, 0, 0) != LUA_OK) {
    checker.disable();
    luaos_throw_error(L);
  }
}

namespace luaos_subscriber
{
  void init_metatable(lua_State* L)
  {
    struct luaL_Reg methods[] = {
      { "subscribe",    lua_os_subscribe  },
      { "cancel",       lua_os_cancel     },
      { "publish",      lua_os_publish    },
      { NULL,           NULL },
    };
    lua_getglobal(L, "os");
    luaL_setfuncs(L, methods, 0);
    lua_pop(L, 1); //pop os from stack
  }
}

/*******************************************************************************/

LUALIB_API int lua_os_subscribe(lua_State* L)
{
  size_t topic = luaL_checkinteger(L, 1);
  if (topic == 0) {
    luaL_argerror(L, 1, "must be an integer greater than 0");
  }

  if (!lua_isfunction(L, 2)) {
    luaL_argerror(L, 2, "must be a function");
  }

  subscriber_item item;
  item.ios   = this_thread().lua_reactor();
  item.index = luaL_ref(L, LUA_REGISTRYINDEX);

  std::unique_lock<std::mutex> lock(_mutex);

  auto iter = _topics.find(topic);
  if (iter == _topics.end())
  {
    item_array new_array;
    new_array.push_back(item);
    _topics[topic] = new_array;
    lua_pushboolean(L, 1);
    return 1;
  }

  item_array& items = iter->second;
  for (size_t i = 0; i < items.size(); i++)
  {
    if (items[i].ios->id() == item.ios->id())
    {
      luaL_unref(L, LUA_REGISTRYINDEX, item.index);
      lua_pushboolean(L, 0);
      return 1;
    }
  }
  items.push_back(item);
  lua_pushboolean(L, 1);
  return 1;
}

LUALIB_API int lua_os_cancel(lua_State* L)
{
  size_t topic = luaL_checkinteger(L, 1);
  if (topic == 0) {
    luaL_argerror(L, 1, "must be an integer greater than 0");
  }

  auto ios = this_thread().lua_reactor();
  std::unique_lock<std::mutex> lock(_mutex);

  auto iter = _topics.find(topic);
  if (iter == _topics.end()) {
    return 0;
  }

  item_array& items = iter->second;
  auto item = items.begin();
  for (; item != items.end(); item++)
  {
    if (item->ios->id() == ios->id())
    {
      luaL_unref(L, LUA_REGISTRYINDEX, item->index);
      items.erase(item);
      break;
    }
  }
  if (items.empty()) {
    _topics.erase(topic);
  }
  return 0;
}

LUALIB_API int lua_os_publish(lua_State* L)
{
  int argc = lua_gettop(L);
  if (argc < 4) {
    luaL_error(L, "too few parameters");
  }

  size_t topic = luaL_checkinteger(L, 1);
  if (topic == 0) {
    luaL_argerror(L, 1, "must be an integer greater than 0");
  }

  size_t mask = luaL_checkinteger(L, 2);
  size_t receiver = luaL_checkinteger(L, 3);

  auto this_ios = this_thread().lua_reactor();
  const int self_id = this_ios->id();
  if (receiver > 0)
  {
    mask = 0;
    if (receiver == self_id) {
      return 0;
    }
  }

  params_type params;
  for (int i = 4; i <= argc; i++) {
    params.push_back(lexget_any(L, i));
  }

  std::unique_lock<std::mutex> lock(_mutex);
  auto iter = _topics.find(topic);
  if (iter == _topics.end()) {
    return 0;
  }

  int self = -1;
  const item_array& items = iter->second;
  for (int i = 0; i < items.size(); i++)
  {
    if (items[i].ios->id() == self_id) {
      self = i;
      break;
    }
  }

  size_t count = items.size() - (self < 0 ? 0 : 1);
  if (count == 0) {
    lua_pushinteger(L, 0);
    return 1;
  }

  if (mask > 0)
  {
    size_t i = mask % count;
    if (self >= 0 && i >= self) {
      i++;
    }
    int index = items[i].index;
    items[i].ios->post(std::bind(&on_publish, self_id, index, params));
    lua_pushinteger(L, 1);
    return 1;
  }

  size_t total = 0;
  auto item = items.begin();
  for (; item != items.end(); item++)
  {
    if (receiver > 0) {
      if (item->ios->id() != receiver) {
        continue;
      }
    }
    total++;
    item->ios->post(std::bind(&on_publish, self_id, item->index, params));
    if (receiver > 0) {
      break;
    }
  }

  lua_pushinteger(L, (lua_Integer)total);
  return 1;
}

/*******************************************************************************/

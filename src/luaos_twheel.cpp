

#include <stdlib.h>
#include <list>
#include <set>
#include <string.h>

#include "luaos.h"
#include "luaos_twheel.h"

/***********************************************************************************/

typedef struct tw_handler
{
  void*  ptw;
  int    handle;
  size_t uuid;
  std::list<std::any> params;
  lua_State* L;
  inline void clear()
  {
    if (!ptw) {
      return;
    }
    if (handle) {
      luaL_unref(L, LUA_REGISTRYINDEX, handle);
      handle = 0;
    }
    ptw = nullptr;
    params.clear();
  }
  inline ~tw_handler() { clear(); }
} tw_handler;

typedef struct
{
  twheel_t* ptw;
  std::set<tw_handler*>* task;
  std::list<tw_handler*>* free;
} timingwheel;

twheel_t* this_timingwheel()
{
  static thread_local struct _G_timingwheel {
    twheel_t* ptw;
    inline _G_timingwheel() {
      ptw = nullptr;
    }

    inline ~_G_timingwheel() {
      timewheel_release(ptw);
    }

    inline twheel_t* create() {
      if (!ptw) ptw = timewheel_create(0);
      return ptw;
    }
  } _G_ptw;
  return _G_ptw.create();
}

static void twheel_cancel(timingwheel* ptw)
{
  if (ptw->task)
  {
    auto iter = ptw->task->begin();
    for (; iter != ptw->task->end(); iter++) {
      (*iter)->clear();
    }

    delete ptw->task;
    ptw->task = nullptr;
  }

  if (ptw->free)
  {
    auto iter = ptw->free->begin();
    for (; iter != ptw->free->end(); iter++) {
      delete* iter;
    }

    delete ptw->free;
    ptw->free = nullptr;
  }
  ptw->ptw = nullptr;
}

static tw_handler* alloc_handler(timingwheel* ptw)
{
  static thread_local size_t nextuuid = 0;
  tw_handler* handler = nullptr;
  if (!ptw->free->empty())
  {
    handler = *ptw->free->begin();
    ptw->free->pop_front();
  }
  else {
    handler = new tw_handler();
  }
  handler->uuid = ++nextuuid;
  return handler;
}

static void free_handler(timingwheel* ptw, tw_handler* handler)
{
  if (ptw)
  {
    handler->clear();
    if (ptw->free)
    {
      ptw->free->push_back(handler);
      return;
    }
  }
  delete handler;
}

/***********************************************************************************/

static void twheel_callback(void* arg)
{
  tw_handler* c = (tw_handler*)arg;
  timingwheel* ptw = (timingwheel*)c->ptw;
  if (ptw)
  {
    ptw->task->erase(c);
    lua_State* L = c->L;
    stack_checker checker(L);

    int handle = c->handle;
    std::list<std::any>& params = c->params;

    lua_pushcfunction(L, lua_pcall_error);
    int error_fn_index = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
    if (lua_isfunction(L, -1))
    {
      auto iter = params.begin();
      for (; iter != params.end(); iter++) {
        lexpush_anyref(L, *iter, true);
      }
      int argc = (int)params.size();
      lua_pcall(L, argc, 0, error_fn_index);
    }
  }
  free_handler(ptw, c);
}

static int twheel_update(lua_State* L)
{
  twheel_t* ptw = this_timingwheel();
  if (ptw) {
    timewheel_update(ptw, luaL_checkinteger(L, 1));
  }
  return 0;
}

static timingwheel* check_self(lua_State* L)
{
  timingwheel* self = lexget_userdata<timingwheel>(L, 1, TWHEEL_NAME);
  if (!self || !self->ptw) {
    return nullptr;
  }
  return self;
}

LUALIB_API int lua_os_twheel_add_time(lua_State* L)
{
  timingwheel* ptw = check_self(L);
  if (!ptw) {
    return 0;
  }

  int index = 2;
  size_t delay = luaL_checkinteger(L, index++);
  if (!lua_isfunction(L, index)) {
    luaL_argerror(L, index, "must be a function");
  }

  tw_handler* c = alloc_handler(ptw);
  if (!c) {
    return 0;
  }

  lua_pushvalue(L, index++);
  c->ptw    = ptw;
  c->handle = luaL_ref(L, LUA_REGISTRYINDEX);
  c->L      = this_thread().lua_state();

  int top = lua_gettop(L);
  for (int i = index; i <= top; i++) {
    c->params.push_back(lexget_anyref(L, i));
  }

  if (timewheel_add_time(ptw->ptw, twheel_callback, (void*)c, (uint32_t)delay))
  {
    ptw->task->insert(c);
    lua_pushinteger(L, (lua_Integer)c->uuid);
    return 1;
  }
  delete c;
  return 0;
}

LUALIB_API int lua_os_twheel_remove(lua_State* L)
{
  timingwheel* ptw = check_self(L);
  if (!ptw) {
    return 0;
  }
  size_t uuid = luaL_checkinteger(L, 2);
  auto iter = ptw->task->begin();
  for (; iter != ptw->task->end(); iter++)
  {
    tw_handler* p = *iter;
    if (p->uuid == uuid)
    {
      p->clear();
      break;
    }
  }
  return 0;
}

LUALIB_API int lua_os_twheel_create(lua_State* L)
{
  timingwheel* userdata = lexnew_userdata<timingwheel>(L, TWHEEL_NAME);
  userdata->ptw = this_timingwheel();
  userdata->task = new std::set<tw_handler*>();
  userdata->free = new std::list<tw_handler*>();
  return 1;
}

LUALIB_API int lua_os_twheel_max_delay(lua_State* L)
{
  timingwheel* ptw = check_self(L);
  if (!ptw) {
    lua_pushnil(L);
  }
  else {
    lua_pushinteger(L, MAX_DELAY_TIME);
  }
  return 1;
}

LUALIB_API int lua_os_twheel_release(lua_State* L)
{
  timingwheel* ptw = check_self(L);
  if (ptw) {
    twheel_cancel(ptw);
  }
  return 0;
}

namespace lua_twheel
{
  void init_metatable(lua_State* L)
  {
    lua_getglobal(L, "os");
    lua_pushcfunction(L, lua_os_twheel_create);
    lua_setfield(L, -2, TWHEEL_NAME);
    lua_pop(L, 1); //pop os from stack

    struct luaL_Reg methods[] = {
      { "__gc",	      lua_os_twheel_release   },
      { "__close",	  lua_os_twheel_release   },
      { "max_delay" , lua_os_twheel_max_delay },
      { "close",      lua_os_twheel_release   },
      { "cancel",     lua_os_twheel_remove    },
      { "scheme",     lua_os_twheel_add_time  },
      { NULL,		      NULL                    }
    };

    lexnew_metatable(L, TWHEEL_NAME, methods);
    lua_pop(L, 1);
  }
}

/***********************************************************************************/

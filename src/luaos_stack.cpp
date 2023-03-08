

#include <list>
#include "luaos_stack.h"
#include "luaos_value.h"

typedef std::list<lua_value> stack_value;

static stack_value* check_metatable(lua_State* L)
{
  return lexget_userdata<stack_value>(L, 1, luaos_stack_name);
}

static int lua_os_stack_new(lua_State* L)
{
  auto* userdata = lexnew_userdata<stack_value>(L, luaos_stack_name);
  new (userdata) stack_value();
  return 1;
}

static int lua_os_stack_gc(lua_State* L)
{
  stack_value* self = check_metatable(L);
  self->~list();
  return 0;
}

static int lua_os_stack_push(lua_State* L)
{
  stack_value* self = check_metatable(L);
  luaL_argcheck(L, !lua_isnone(L, 2), 2, "missing value");
  self->push_front(lua_value(L, 2));
  lua_pushinteger(L, (lua_Integer)self->size());
  return 1;
}

static int lua_os_stack_pop(lua_State* L)
{
  stack_value* self = check_metatable(L);
  if (self->empty()) {
    lua_pushnil(L);
  }
  else {
    auto value = self->front();
    self->pop_front();
    value.push(L);
  }
  return 1;
}

static int lua_os_stack_size(lua_State* L)
{
  stack_value* self = check_metatable(L);
  lua_pushinteger(L, (lua_Integer)self->size());
  return 1;
}

static int lua_os_stack_clear(lua_State* L)
{
  stack_value* self = check_metatable(L);
  self->clear();
  return 0;
}

static void init_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",         lua_os_stack_gc     },
    { "push",         lua_os_stack_push   },
    { "pop",          lua_os_stack_pop    },
    { "size",         lua_os_stack_size   },
    { "clear",        lua_os_stack_clear  },
    { NULL,           NULL },
  };
  lexnew_metatable(L, luaos_stack_name, methods);
  lua_pop(L, 1);
}

/***********************************************************************************/

int luaopen_stack(lua_State* L)
{
  luaL_checkversion(L);
  init_metatable(L);
  lua_pushcfunction(L, lua_os_stack_new);
  return 1;
}

/***********************************************************************************/



#include <list>
#include "luaos_list.h"
#include "luaos_value.h"

typedef std::list<lua_value> stack_value;

static stack_value* check_metatable(lua_State* L)
{
  return lexget_userdata<stack_value>(L, 1, luaos_list_name);
}

static int lua_os_list_new(lua_State* L)
{
  auto* userdata = lexnew_userdata<stack_value>(L, luaos_list_name);
  new (userdata) stack_value();
  return 1;
}

static int lua_os_list_gc(lua_State* L)
{
  stack_value* self = check_metatable(L);
  self->~list();
  return 0;
}

static int lua_os_list_front(lua_State* L)
{
  stack_value* self = check_metatable(L);
  if (self->empty()) {
    lua_pushnil(L);
  }
  else {
    self->front().push(L);
  }
  return 1;
}

static int lua_os_list_push_front(lua_State* L)
{
  stack_value* self = check_metatable(L);
  luaL_argcheck(L, !lua_isnone(L, 2), 2, "missing value");
  self->push_front(lua_value(L, 2));
  lua_pushinteger(L, (lua_Integer)self->size());
  return 1;
}

static int lua_os_list_pop_front(lua_State* L)
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

static int lua_os_list_back(lua_State* L)
{
  stack_value* self = check_metatable(L);
  if (self->empty()) {
    lua_pushnil(L);
  }
  else {
    self->back().push(L);
  }
  return 1;
}

static int lua_os_list_push_back(lua_State* L)
{
  stack_value* self = check_metatable(L);
  luaL_argcheck(L, !lua_isnone(L, 2), 2, "missing value");
  self->push_back(lua_value(L, 2));
  lua_pushinteger(L, (lua_Integer)self->size());
  return 1;
}

static int lua_os_list_pop_back(lua_State* L)
{
  stack_value* self = check_metatable(L);
  if (self->empty()) {
    lua_pushnil(L);
  }
  else {
    auto value = self->back();
    self->pop_back();
    value.push(L);
  }
  return 1;
}

static int lua_os_list_size(lua_State* L)
{
  stack_value* self = check_metatable(L);
  lua_pushinteger(L, (lua_Integer)self->size());
  return 1;
}

static int lua_os_list_clear(lua_State* L)
{
  stack_value* self = check_metatable(L);
  self->clear();
  return 0;
}

static int lua_os_list_iterator_gc(lua_State* L)
{
  auto iter = lexget_userdata<stack_value::iterator*>(
    L, 1, luaos_list_iterator
  );
  if (iter && *iter) {
    delete *iter;
    *iter = nullptr;
  }
  return 0;
}

static int lua_os_list_iterator(lua_State* L)
{
  auto list = (stack_value*)lua_touserdata(
    L, lua_upvalueindex(1)
  );
  if (!list) {
    luaL_error(L, "self invalid");
    return 0;
  }
  auto iter = *(stack_value::iterator**)lua_touserdata(
    L, lua_upvalueindex(2)
  );
  if (!iter) {
    luaL_error(L, "iterator invalid");
    return 0;
  }
  if (*iter == list->end()) {
    return 0;
  }
  size_t size = lua_tointeger(L, lua_upvalueindex(3));
  if (list->size() != size) {
    luaL_error(L, "traversing process cannot delete elements");
    return 0;
  }
  (*iter)->push(L);
  (*iter)++;
  return 1;
}

static int lua_os_list_pairs(lua_State* L)
{
  stack_value* self = check_metatable(L);
  lua_pushlightuserdata(L, self);

  auto iter = lexnew_userdata<stack_value::iterator*>(L, luaos_list_iterator);
  *iter = new stack_value::iterator(self->begin());

  lua_pushinteger(L, (lua_Integer)self->size());
  lua_pushcclosure(L, lua_os_list_iterator, 3);
  return 1;
}

static void init_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",         lua_os_list_gc          },
    { "__pairs",      lua_os_list_pairs       },
    { "__len",        lua_os_list_size        },
    { "front",        lua_os_list_front       },
    { "push_front",   lua_os_list_push_front  },
    { "pop_front",    lua_os_list_pop_front   },
    { "back",         lua_os_list_back        },
    { "push_back",    lua_os_list_push_back   },
    { "pop_back",     lua_os_list_pop_back    },
    { "size",         lua_os_list_size        },
    { "clear",        lua_os_list_clear       },
    { NULL,           NULL                    },
  };
  lexnew_metatable(L, luaos_list_name, methods);
  lua_pop(L, 1);

  struct luaL_Reg iterator[] = {
    { "__gc",         lua_os_list_iterator_gc },
    { NULL,           NULL                    },
  };
  lexnew_metatable(L, luaos_list_iterator, iterator);
  lua_pop(L, 1);
}

/***********************************************************************************/

int luaopen_list(lua_State* L)
{
  luaL_checkversion(L);
  init_metatable(L);
  lua_pushcfunction(L, lua_os_list_new);
  return 1;
}

/***********************************************************************************/

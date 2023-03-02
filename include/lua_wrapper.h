

/************************************************************************************
**
** Copyright 2021 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
************************************************************************************/

#pragma once

#ifndef __cplusplus
# error Sorry, luaext.h can only be used with C++
#endif

#include <string.h>
#include <string>
#include <cctype>
#include <vector>
#include <algorithm>
#include <wrapper/lua_value.h>

/***********************************************************************************/
//Compatibility adaptation

/* for Lua 5.0 */
#if !defined LUA_VERSION_NUM
# define luaL_Reg luaL_reg
# define lua_pushinteger(L, n) lua_pushnumber(L, (lua_Number)n)
#elif LUA_VERSION_NUM > 501
#if !defined(luaL_checkint)
inline int luaL_checkint(lua_State* L, int index) {
  return (int)luaL_checkinteger(L, index);
}
#endif
#if !defined(lua_strlen)
inline size_t lua_strlen(lua_State* L, int index) {
  size_t size = 0;
  luaL_checklstring(L, index, &size);
  return size;
}
#endif
#endif

/* for Lua 5.2 and lower */
#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM < 503
# define lua_isinteger(L, n) (!lua_isnoneornil(L, n) && (luaL_checknumber(L, n) == luaL_checkinteger(L, n)) ? 1 : 0)
#endif

/* for Lua 5.1 and lower */
#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM == 501
# define luaL_setfuncs lexset_methods
# define lua_rotate(L, i, n) lua_pushvalue(L, i),lua_remove(L, i - n)
#endif

/***********************************************************************************/

#ifndef  lexset_table
# define lexset_table(L, key_type, key, value_type, value) \
  lua_push##key_type(L, key);                              \
	lua_push##value_type(L, value);                          \
	lua_settable(L, -3);
#endif

struct lua_enum { const char* name; const int value; };

/***********************************************************************************/

/*
** Read any type of data on stack
*/
inline lua_value luaL_getvalue(lua_State* L, int index, int level = 0)
{
  lua_value value;
  if (lua_isnoneornil(L, index)) {
    return value;
  }
  if (index < 0) {
    index = lua_gettop(L) + 1 + index;
  }
  auto type = lua_type(L, index);
  if (type == LUA_TNUMBER) {
    if (lua_isinteger(L, index)) {
      return lua_tointeger(L, index);
    }
    return lua_tonumber(L, index);
  }
  if (type == LUA_TSTRING) {
    size_t size = 0;
    const char* data = lua_tolstring(L, index, &size);
    return std::string(data, size);
  }
  if (type == LUA_TNONE || type == LUA_TTABLE) {
    return lua_table::create(L, index);
  }
  if (type == LUA_TBOOLEAN) {
    return lua_toboolean(L, index) ? true : false;
  }
  if (type == LUA_TFUNCTION) {
    lua_function luafn;
    luafn.ref(L, index);
    return luafn;
  }
  if (type == LUA_TUSERDATA) {
    return lua_touserdata(L, index);
  }
  return value;
}

/*
** Push any type of data on stack
*/
inline void luaL_pushvalue(lua_State* L, const lua_value& value) {
  if (!value.has_value()) {
    lua_pushnil(L);
    return;
  }
  auto type = value.type();
  if (type == LUA_TNUMBER) {
    if (value.is_integer()) {
      lua_pushinteger(L, (lua_Integer)value);
      return;
    }
    lua_pushnumber(L, (lua_Number)value);
    return;
  }
  if (type == LUA_TSTRING) {
    std::string data = value;
    lua_pushlstring(L, data.c_str(), data.size());
    return;
  }
  if (type == LUA_TTABLE) {
    lua_table::value_type tbl = value;
    tbl->push(L);
    return;
  }
  if (type == LUA_TBOOLEAN) {
    lua_pushboolean(L, ((bool)value) ? 1 : 0);
    return;
  }
  if (type == LUA_TFUNCTION) {
    lua_function luafn;
    luafn.rawget(L);
    return;
  }
  if (type == LUA_TUSERDATA) {
    lua_pushlightuserdata(L, (void*)value);
    return;
  }
  lua_pushnil(L);
}

template <typename... Args>
inline void luaL_pushparams(lua_State* L, Args... args) {
  lua_value params[] = { args... };
  for (size_t i = 0; i < sizeof...(args); i++) {
    luaL_pushvalue(L, params[i]);
  }
}

/***********************************************************************************/

/*
** Get table type parameters on stack
** if there is any error, lexget_table catches it
*/
inline lua_table::value_type luaL_gettable(lua_State* L, int index) {
  luaL_argcheck(L, lua_istable(L, index), index, "table expected");
  return lua_table::create(L, index);
}

/*
** Get optional table type parameters on stack
** if there is any error, lexget_opttable catches it
*/
inline lua_table::value_type luaL_opttable(lua_State* L, int index, lua_table::value_type def) {
  return lua_isnoneornil(L, index) ? def : luaL_gettable(L, index);
}

/*
** Get boolean type parameters on stack
** if there is any error, lexget_boolean catches it
*/
inline bool luaL_getboolean(lua_State* L, int index) {
  return lua_toboolean(L, index) ? true : false;
}

/*
** Get optional boolean type parameters on stack
** if there is any error, lexget_optboolean catches it
*/
inline bool luaL_optboolean(lua_State* L, int index, bool def = false) {
  return lua_isnoneornil(L, index) ? def : luaL_getboolean(L, index);
}

/*
** Set metatable for table or userdata
*/
inline void lexset_metatable(lua_State* L, const char* name) {
  luaL_getmetatable(L, name);
  lua_setmetatable(L, -2);
}

/*
** Set enum for table or metatable
*/
inline void lexset_enum(lua_State* L, const lua_enum* e) {
  for (; e->name; ++e) {
    luaL_checkstack(L, 1, "enum too large");
    lua_pushinteger(L, e->value);
    lua_setfield(L, -2, e->name);
  }
}

/*
** Set methods for table or metatable
*/
inline void lexset_methods(lua_State* L, const luaL_Reg* reg, int index) {
  luaL_checkstack(L, index + 1, "too many upvalues");
  /* fill the table with given functions */
  for (; reg->name != 0; reg++) {
    int i;
    lua_pushstring(L, reg->name);
    /* copy upvalues to the top */
    for (i = 0; i < index; i++) {
      lua_pushvalue(L, -(index + 1));
    }
    /* closure with those upvalues */
    lua_pushcclosure(L, reg->func, index);
    lua_settable(L, -(index + 3));
  }
  /* remove upvalues */
  lua_pop(L, index);
}

/*
** Create a new metatable
*/
inline int lexnew_metatable(lua_State* L, const char* name, const luaL_Reg* methods) {
  if (!luaL_newmetatable(L, name)) {
    return 0;
  }
  /* define methods */
  luaL_setfuncs(L, methods, 0);

  /* define metamethods */
  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);
  lua_settable(L, -3);

  lua_pushliteral(L, "__metatable");
  lua_pushliteral(L, "you're not allowed to get this metatable");
  lua_settable(L, -3);
  return 1;
}

/*
** Create a new userdata and set metatable
*/
inline void* lexnew_userdata(lua_State* L, const char* name, size_t size) {
  void* userdata = lua_newuserdata(L, size);
  lexset_metatable(L, name);
  return userdata;
}

/*
** Create a new userdata and set metatable
*/
template <typename _Ty>
inline _Ty* lexnew_userdata(lua_State* L, const char* name) {
  return (_Ty*)lexnew_userdata(L, name, sizeof(_Ty));
}

/*
** Get and check userdata from stack
** if there is any error, lexget_userdata catches it
*/
template <typename _Ty>
inline _Ty* lexget_userdata(lua_State* L, int index, const char* name) {
  _Ty* userdata = (_Ty*)luaL_checkudata(L, index, name);
  luaL_argcheck(L, userdata != NULL, index, "userdata value expected");
  return userdata;
}

/***********************************************************************************/

template<typename T>
struct reference_traits_t {
  typedef T arg_type_t;
};

template<>
struct reference_traits_t<const std::string&> {
  typedef std::string arg_type_t;
};

template<>
struct reference_traits_t<std::string&> {
  typedef std::string arg_type_t;
};

template<typename T>
struct reference_traits_t<const T*> {
  typedef T* arg_type_t;
};
template<typename T>
struct reference_traits_t<const T&> {
  typedef T arg_type_t;
};

template <typename T>
struct init_value_traits_t {
  inline static T value(){ return T(); }
};

template <typename T>
struct init_value_traits_t<const T*> {
  inline static T* value(){ return NULL; }
};

template <typename T>
struct init_value_traits_t<const T&> {
  inline static T value(){ return T(); }
};

template <>
struct init_value_traits_t<std::string> {
  inline static const char* value(){ return ""; }
};

template <>
struct init_value_traits_t<const std::string&> {
  inline static const char* value(){ return ""; }
};

/***********************************************************************************/

template <typename T> struct lua_CFunction_param {};

template<>
struct lua_CFunction_param<bool> {
  static bool get(lua_State* L, int i) {
    return lua_toboolean(L, i) ? true : false;
  }
  static bool get(lua_State* L, int i, bool def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, bool v) {
    lua_pushboolean(L, v ? 1 : 0);
  }
};

template<>
struct lua_CFunction_param<char> {
  static char get(lua_State* L, int i) {
    return (char)luaL_checkinteger(L, i);
  }
  static char get(lua_State* L, int i, char def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, char v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<unsigned char> {
  static unsigned char get(lua_State* L, int i) {
    return (unsigned char)luaL_checkinteger(L, i);
  }
  static unsigned char get(lua_State* L, int i, unsigned char def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, unsigned char v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<short> {
  static short get(lua_State* L, int i) {
    return (short)luaL_checkinteger(L, i);
  }
  static short get(lua_State* L, int i, short def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, short v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<unsigned short> {
  static unsigned short get(lua_State* L, int i) {
    return (unsigned short)luaL_checkinteger(L, i);
  }
  static unsigned short get(lua_State* L, int i, unsigned short def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, unsigned short v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<int> {
  static int get(lua_State* L, int i) {
    return (int)luaL_checkinteger(L, i);
  }
  static int get(lua_State* L, int i, int def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, int v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<unsigned int> {
  static unsigned int get(lua_State* L, int i) {
    return (unsigned int)luaL_checkinteger(L, i);
  }
  static unsigned int get(lua_State* L, int i, unsigned int def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, unsigned int v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<long> {
  static long get(lua_State* L, int i) {
    return (long)luaL_checkinteger(L, i);
  }
  static long get(lua_State* L, int i, long def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, long v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<unsigned long> {
  static unsigned long get(lua_State* L, int i) {
    return (unsigned long)luaL_checkinteger(L, i);
  }
  static unsigned long get(lua_State* L, int i, unsigned long def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, unsigned long v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<long long> {
  static long long get(lua_State* L, int i) {
    return (long long)luaL_checkinteger(L, i);
  }
  static long long get(lua_State* L, int i, long long def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, long long v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<unsigned long long> {
  static unsigned long long get(lua_State* L, int i) {
    return (size_t)luaL_checkinteger(L, i);
  }
  static unsigned long long get(lua_State* L, int i, unsigned long long def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, unsigned long long v) {
    lua_pushinteger(L, (lua_Integer)v);
  }
};

template<>
struct lua_CFunction_param<float> {
  static float get(lua_State* L, int i) {
    return (float)luaL_checknumber(L, i);
  }
  static float get(lua_State* L, int i, float def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, float v) {
    lua_pushnumber(L, (lua_Number)v);
  }
};

template<>
struct lua_CFunction_param<double> {
  static double get(lua_State* L, int i) {
    return (double)luaL_checknumber(L, i);
  }
  static double get(lua_State* L, int i, double def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, double v) {
    lua_pushnumber(L, (lua_Number)v);
  }
};

template<>
struct lua_CFunction_param<char*> {
  static char* get(lua_State* L, int i) {
    return (char*)luaL_checkstring(L, i);
  }
  static char* get(lua_State* L, int i, char* def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, char* v) {
    if (!v) {
      lua_pushnil(L);
    } else {
      lua_pushstring(L, v);
    }
  }
};

template<>
struct lua_CFunction_param<const char*> {
  static const char* get(lua_State* L, int i) {
    return (const char*)luaL_checkstring(L, i);
  }
  static const char* get(lua_State* L, int i, const char* def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, const char* v) {
    if (!v) {
      lua_pushnil(L);
    } else {
      lua_pushstring(L, v);
    }
  }
};

template<>
struct lua_CFunction_param<void*> {
  static void* get(lua_State* L, int i) {
    return (void*)lua_touserdata(L, i);
  }
  static void* get(lua_State* L, int i, void* def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, void* v) {
    if (!v) {
      lua_pushnil(L);
    } else {
      lua_pushlightuserdata(L, v);
    }
  }
};

template<>
struct lua_CFunction_param<const void*> {
  static const void* get(lua_State* L, int i) {
    return (const void*)lua_touserdata(L, i);
  }
  static const void* get(lua_State* L, int i, const void* def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, const void* v) {
    if (!v) {
      lua_pushnil(L);
    } else {
      lua_pushlightuserdata(L, (void*)v);
    }
  }
};

template<>
struct lua_CFunction_param<std::string> {
  static std::string get(lua_State* L, int i) {
    return std::string(luaL_checkstring(L, i));
  }
  static std::string get(lua_State* L, int i, const std::string& def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, const std::string& v) {
    lua_pushlstring(L, v.c_str(), v.size());
  }
};

template<>
struct lua_CFunction_param<std::string&> {
  static std::string get(lua_State* L, int i) {
    return luaL_checkstring(L, i);
  }
  static std::string get(lua_State* L, int i, const std::string& def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, const std::string& v) {
    lua_pushlstring(L, v.c_str(), v.size());
  }
};

template<>
struct lua_CFunction_param<const std::string&> {
  static std::string get(lua_State* L, int i) {
    return luaL_checkstring(L, i);
  }
  static std::string get(lua_State* L, int i, const std::string& def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, const std::string& v) {
    lua_pushlstring(L, v.c_str(), v.size());
  }
};

/***********************************************************************************/

template <typename Handler>
struct lua_CFunction_userdata_t {
  lua_CFunction_userdata_t(Handler fn) : cfunction(fn) { }
  Handler cfunction;
};

template <typename Handler>
struct lua_CFunction_wrapper
{
  typedef void (*dest_func_t)();
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn();
    return 0;
  }
};

template <typename RET>
struct lua_CFunction_wrapper<RET (*)()>
{
  typedef RET (*dest_func_t)();
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn());
    return 1;
  }
};

template <typename Arg1>
struct lua_CFunction_wrapper<void(*)(Arg1)>
{
  typedef void (*dest_func_t)(Arg1);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1);
    return 0;
  }
};

template <typename RET, typename Arg1>
struct lua_CFunction_wrapper<RET (*)(Arg1)>
{
  typedef RET (*dest_func_t)(Arg1);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1));
    return 1;
  }
};

template <typename Arg1, typename Arg2>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2)>
{
  typedef void (*dest_func_t)(Arg1, Arg2);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  typedef typename reference_traits_t<Arg1>::arg_type_t arg_type_t;

  static int lua_function(lua_State* L)
  {
    int i = 1;
    arg_type_t def1 = init_value_traits_t<Arg1>::value();
    arg_type_t def2 = init_value_traits_t<Arg2>::value();
    arg_type_t def3 = init_value_traits_t<Arg3>::value();
    Arg1 arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    Arg2 arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    Arg2 arg3 = lua_CFunction_param<Arg2>::get(L, i++, def3);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg2>::get(L, i++, def3);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3, Arg4)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3, Arg4);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, arg1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, arg2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, arg3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, arg4);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3, arg4);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3, Arg4)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3, Arg4);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, arg1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, arg2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, arg3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, arg4);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3, arg4));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3, Arg4, Arg5)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3, arg4, arg5);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3, Arg4, Arg5)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3, arg4, arg5));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3, arg4, arg5, arg6);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3, arg4, arg5, arg6));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto def7 = init_value_traits_t<Arg7>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);
    auto arg7 = lua_CFunction_param<Arg7>::get(L, i++, def7);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto def7 = init_value_traits_t<Arg7>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);
    auto arg7 = lua_CFunction_param<Arg7>::get(L, i++, def7);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3, arg4, arg5, arg6, arg7));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto def7 = init_value_traits_t<Arg7>::value();
    auto def8 = init_value_traits_t<Arg8>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);
    auto arg7 = lua_CFunction_param<Arg7>::get(L, i++, def7);
    auto arg8 = lua_CFunction_param<Arg8>::get(L, i++, def8);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto def7 = init_value_traits_t<Arg7>::value();
    auto def8 = init_value_traits_t<Arg8>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);
    auto arg7 = lua_CFunction_param<Arg7>::get(L, i++, def7);
    auto arg8 = lua_CFunction_param<Arg8>::get(L, i++, def8);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8));
    return 1;
  }
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct lua_CFunction_wrapper<void(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9)>
{
  typedef void (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto def7 = init_value_traits_t<Arg7>::value();
    auto def8 = init_value_traits_t<Arg8>::value();
    auto def9 = init_value_traits_t<Arg9>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);
    auto arg7 = lua_CFunction_param<Arg7>::get(L, i++, def7);
    auto arg8 = lua_CFunction_param<Arg8>::get(L, i++, def8);
    auto arg9 = lua_CFunction_param<Arg9>::get(L, i++, def9);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    fn(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    return 0;
  }
};

template <typename RET, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct lua_CFunction_wrapper<RET(*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9)>
{
  typedef RET (*dest_func_t)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
  typedef lua_CFunction_userdata_t<dest_func_t> lua_CFunction_userdata;
  static int lua_function(lua_State* L)
  {
    int i = 1;
    auto def1 = init_value_traits_t<Arg1>::value();
    auto def2 = init_value_traits_t<Arg2>::value();
    auto def3 = init_value_traits_t<Arg3>::value();
    auto def4 = init_value_traits_t<Arg4>::value();
    auto def5 = init_value_traits_t<Arg5>::value();
    auto def6 = init_value_traits_t<Arg6>::value();
    auto def7 = init_value_traits_t<Arg7>::value();
    auto def8 = init_value_traits_t<Arg8>::value();
    auto def9 = init_value_traits_t<Arg9>::value();
    auto arg1 = lua_CFunction_param<Arg1>::get(L, i++, def1);
    auto arg2 = lua_CFunction_param<Arg2>::get(L, i++, def2);
    auto arg3 = lua_CFunction_param<Arg3>::get(L, i++, def3);
    auto arg4 = lua_CFunction_param<Arg4>::get(L, i++, def4);
    auto arg5 = lua_CFunction_param<Arg5>::get(L, i++, def5);
    auto arg6 = lua_CFunction_param<Arg6>::get(L, i++, def6);
    auto arg7 = lua_CFunction_param<Arg7>::get(L, i++, def7);
    auto arg8 = lua_CFunction_param<Arg8>::get(L, i++, def8);
    auto arg9 = lua_CFunction_param<Arg9>::get(L, i++, def9);

    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    dest_func_t& fn = *((dest_func_t*)ud);
    lua_CFunction_param<RET>::set(L, fn(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9));
    return 1;
  }
};

/***********************************************************************************/

template <typename Handler>
static void lua_pushchandler(lua_State* L, Handler fn)
{
  typedef typename lua_CFunction_wrapper<Handler>::lua_CFunction_userdata lua_CFunction_userdata;
  auto ud = lua_newuserdata(L, sizeof(lua_CFunction_userdata));
  new (ud) lua_CFunction_userdata(fn);
  lua_pushcclosure(L, lua_CFunction_wrapper<Handler>::lua_function, 1);
}

/***********************************************************************************/

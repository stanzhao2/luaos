

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
#include <wrapper/lex_type.h>

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

#ifndef  LUA_OK
# define LUA_OK 0
#endif

/* for Lua 5.1 and lower */
#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM == 501
# define luaL_setfuncs lexset_methods
# define lua_rotate(L, i, n) lua_pushvalue(L, i),lua_remove(L, i - n)
#endif

#define WIN_DSCHR       "\\"
#define POSIX_DSCHR     "/"

#if defined(_WIN32) || defined(WIN32)
# define DSCHR WIN_DSCHR
#else
# define DSCHR POSIX_DSCHR
#endif

/***********************************************************************************/

#define type_cast std::any_cast

#ifndef  lexset_table
# define lexset_table(L, key_type, key, value_type, value) \
  lua_push##key_type(L, key);                            \
	lua_push##value_type(L, value);                        \
	lua_settable(L, -3);
#endif

struct lua_enum {
  const char* name;
  const int value;
};

class lua_exception : public std::exception {
  std::string m_err;
public:
  explicit lua_exception(const char* err_) : m_err(err_) {}
  explicit lua_exception(const std::string& err_) : m_err(err_) {}
  const char* what() const throw () { return m_err.c_str(); }
};

extern std::any lexget_any(lua_State* L, int index, int level);
extern void lexpush_any(lua_State* L, const std::any& value);

/***********************************************************************************/

/*
** Internal use, do not call directly
*/
inline std::string lexpop_string(lua_State* L, int index)
{
  size_t len = 0;
  const char* str = lua_tolstring(L, index, &len);
  return std::move(std::string(str, len));
}

/*
** Internal use, do not call directly
*/
inline void lexpop_error(lua_State* L, int index)
{
  luaL_traceback(L, L, lua_tostring(L, index), 2);
  lua_remove(L, -2);
  throw lua_exception(luaL_checkstring(L, -1));
}

/*
** Internal use, do not call directly
*/
inline void lexarg_error(lua_State* L, int index, const char* err)
{
  char buf[256];
  const char* got = luaL_typename(L, index);
  sprintf(buf, "%s, got %s", err, got);
  luaL_argerror(L, index, buf);
}

/*
** Internal use, do not call directly
*/
inline lua_function lexpop_function(lua_State* L, int index)
{
  lua_pushvalue(L, index);
  lua_function func = luaL_ref(L, LUA_REGISTRYINDEX);
  return func;
}

/*
** Internal use, do not call directly
*/
inline lua_table<> lexpop_table(lua_State* L, int index, int level = 0)
{
  if (level > 32) {
    luaL_error(L, "table nesting level is too deep");
  }
  lua_table<> table;
  lua_pushnil(L);
  while (lua_next(L, index))
  {
    std::any v = lexget_any(L, lua_gettop(L), level);
    lua_pop(L, 1);
    int ltype = lua_type(L, -1);
    if (ltype == LUA_TNUMBER) {
      table[lua_tointeger(L, -1)] = std::move(v);
    }
    else if (ltype == LUA_TSTRING) {
      table[lua_tostring(L, -1)] = std::move(v);
    }
    else {
      lexarg_error(L, index, "unsupported key type");
    }
  }
  return table;
}

/*
** Internal use, do not call directly
*/
inline void lexpush_string(lua_State* L, const std::string* str)
{
  lua_pushlstring(L, str->c_str(), str->size());
}

/*
** Internal use, do not call directly
*/
inline void lexpush_table(lua_State* L, const lua_table<>* table)
{
  lua_newtable(L);
  lua_checkstack(L, (int)table->size());
  for (auto it = table->begin(); it != table->end(); ++it) {
    if (it->first.is_number_key()) {
      lua_pushinteger(L, it->first.index);
    }
    else {
      lua_pushlstring(L, it->first.name.c_str(), it->first.name.size());
    }
    lexpush_any(L, it->second);
    lua_rawset(L, -3);
  }
}

/*
** Internal use, do not call directly
*/
inline void lexpush_function(lua_State* L, const lua_function& func)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, func);
}

/***********************************************************************************/

/*
** Read any type of data on stack
*/
inline std::any lexget_any(lua_State* L, int index, int level = 0)
{
  std::any value;
  if (lua_isnoneornil(L, index)) {
    return value;
  }
  if (index < 0) {
    index = lua_gettop(L) + 1 + index;
  }
  switch (lua_type(L, index))
  {
  case LUA_TNONE:
    value = lua_table<>();
    break;
  case LUA_TBOOLEAN:
    value = lua_toboolean(L, index) ? true : false;
    break;
  case LUA_TNUMBER:
    if (lua_isinteger(L, index)) {
      value = lua_tointeger(L, index);
    }
    else {
      value = lua_tonumber(L, index);
    }
    break;
  case LUA_TSTRING:
    value = lexpop_string(L, index);
    break;
  case LUA_TFUNCTION:
    value = lexpop_function(L, index);
    break;
  case LUA_TUSERDATA:
    value = lua_touserdata(L, index);
    break;
  case LUA_TTABLE:
    value = lexpop_table(L, index, ++level);
    break;
  }
  return value;
}

inline std::any lexget_anyref(lua_State* L, int index)
{
  std::any value;
  if (lua_isnoneornil(L, index)) {
    return value;
  }
  if (index < 0) {
    index = lua_gettop(L) + 1 + index;
  }
  lua_pushvalue(L, index);
  return luaL_ref(L, LUA_REGISTRYINDEX);
}

/*
** Push any type of data on stack
*/
inline void lexpush_any(lua_State* L, const std::any& value)
{
  if (!value.has_value()) {
    lua_pushnil(L);
    return;
  }
  c_type type = type_id(value.type());
  switch (type)
  {
  case c_type::i_void_ptr:
    lua_pushlightuserdata(L, type_cast<void*>(value));
    break;
  case c_type::i_char_ptr:
    lua_pushstring (L, type_cast<const char*>(value));
    break;
  case c_type::i_bool:
    lua_pushboolean(L, type_cast<bool>(value));
    break;
  case c_type::i_char:
    lua_pushinteger(L, type_cast<char>(value));
    break;
  case c_type::i_short:
    lua_pushinteger(L, type_cast<short>(value));
    break;
  case c_type::i_int:
    lua_pushinteger(L, type_cast<int>(value));
    break;
  case c_type::i_long:
    lua_pushinteger(L, type_cast<long>(value));
    break;
  case c_type::i_float:
    lua_pushnumber(L, type_cast<float>(value));
    break;
  case c_type::i_double:
    lua_pushnumber(L, type_cast<double>(value));
    break;
  case c_type::i_longlong:
    lua_pushinteger(L, type_cast<long long>(value));
    break;
  case c_type::i_uchar:
    lua_pushinteger(L, type_cast<unsigned char>(value));
    break;
  case c_type::i_ushort:
    lua_pushinteger(L, type_cast<unsigned short>(value));
    break;
  case c_type::i_uint:
    lua_pushinteger(L, type_cast<unsigned int>(value));
    break;
  case c_type::i_size_t:
    lua_pushinteger(L, type_cast<size_t>(value));
    break;
  case c_type::i_ulonglong:
    lua_pushinteger(L, type_cast<unsigned long long>(value));
    break;
  case c_type::i_lua_number:
    lua_pushnumber(L, type_cast<lua_Number>(value));
    break;
  case c_type::i_lua_integer:
    lua_pushinteger(L, type_cast<lua_Integer>(value));
    break;
  case c_type::i_string:
    lexpush_string(L, type_cast<std::string>(&value));
    break;
  case c_type::i_lua_table:
    lexpush_table(L, type_cast<lua_table<>>(&value));
    break;
  case c_type::i_lua_function:
    lexpush_function(L, type_cast<lua_function>(value));
    break;
  default:
    lua_pushnil(L);
  }
}

inline void lexpush_anyref(lua_State* L, const std::any& value, bool unref = true)
{
  c_type type = type_id(value.type());
  if (type != c_type::i_int) {
    lua_pushnil(L);
    return;
  }
  int index = type_cast<int>(value);
  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  if (unref) {
    luaL_unref(L, LUA_REGISTRYINDEX, index);
  }
  return;
}

template <typename... Args>
inline void lexpush_params(lua_State* L, Args... args)
{
  std::any params[] = { args... };
  for (size_t i = 0; i < sizeof...(args); i++) {
    lexpush_any(L, params[i]);
  }
}

/***********************************************************************************/

/*
** Get function type parameters on stack
** if there is any error, lexget_function catches it
*/
inline lua_function lexget_function(lua_State* L, int index)
{
  if (!lua_isfunction(L, index)) {
    lexarg_error(L, index, "function expected");
  }
  auto value = lexget_any(L, index);
  return std::move(type_cast<lua_function>(value));
}

/*
** Get optional function type parameters on stack
** if there is any error, lexget_optfunction catches it
*/
inline lua_function lexget_optfunction(lua_State* L, int index, lua_function def = lua_function())
{
  return lua_isnoneornil(L, index) ? def : lexget_function(L, index);
}

/*
** Get table type parameters on stack
** if there is any error, lexget_table catches it
*/
inline lua_table<> lexget_table(lua_State* L, int index)
{
  if (!lua_istable(L, index)) {
    lexarg_error(L, index, "table expected");
  }
  auto value = lexget_any(L, index);
  return std::move(type_cast<lua_table<>>(value));
}

/*
** Get optional table type parameters on stack
** if there is any error, lexget_opttable catches it
*/
inline lua_table<> lexget_opttable(lua_State* L, int index, lua_table<> def = lua_table<>())
{
  return lua_isnoneornil(L, index) ? def : lexget_table(L, index);
}

/*
** Get integer type parameters on stack
** if there is any error, lexget_integer catches it
*/
inline lua_Integer lexget_integer(lua_State* L, int index)
{
  return (lua_Integer)luaL_checkinteger(L, index);
}

/*
** Get optional integer type parameters on stack
** if there is any error, lexget_optinteger catches it
*/
inline lua_Integer lexget_optinteger(lua_State* L, int index, lua_Integer def = 0)
{
  return lua_isnoneornil(L, index) ? def : lexget_integer(L, index);
}

/*
** Get number type parameters on stack
** if there is any error, lexget_number catches it
*/
inline lua_Number lexget_number(lua_State* L, int index)
{
  return (lua_Number)luaL_checknumber(L, index);
}

/*
** Get optional number type parameters on stack
** if there is any error, lexget_optnumber catches it
*/
inline lua_Number lexget_optnumber(lua_State* L, int index, lua_Number def = 0.0f)
{
  return lua_isnoneornil(L, index) ? def : lexget_number(L, index);
}

/*
** Get string type parameters on stack
** if there is any error, lexget_string catches it
*/
inline std::string lexget_string(lua_State* L, int index)
{
  size_t size = 0;
  const char* data = lua_tolstring(L, index, &size);
  return std::move(std::string(data, size));
}

/*
** Get optional string type parameters on stack
** if there is any error, lexget_optstring catches it
*/
inline std::string lexget_optstring(lua_State* L, int index, const std::string def = "")
{
  return lua_isnoneornil(L, index) ? def : lexget_string(L, index);
}

/*
** Get boolean type parameters on stack
** if there is any error, lexget_boolean catches it
*/
inline bool lexget_boolean(lua_State* L, int index)
{
  if (!lua_isboolean(L, index)) {
    lexarg_error(L, index, "boolean expected");
  }
  return lua_toboolean(L, index) ? true : false;
}

/*
** Get optional boolean type parameters on stack
** if there is any error, lexget_optboolean catches it
*/
inline bool lexget_optboolean(lua_State* L, int index, bool def = false)
{
  return lua_isnoneornil(L, index) ? def : lexget_boolean(L, index);
}

/***********************************************************************************/

/*
** Set metatable for table or userdata
*/
inline void lexset_metatable(lua_State* L, const char* name)
{
  luaL_getmetatable(L, name);
  lua_setmetatable(L, -2);
}

/*
** Set enum for table or metatable
*/
inline void lexset_enum(lua_State* L, const lua_enum* e)
{
  for (; e->name; ++e) {
    luaL_checkstack(L, 1, "enum too large");
    lua_pushinteger(L, e->value);
    lua_setfield(L, -2, e->name);
  }
}

/*
** Set methods for table or metatable
*/
inline void lexset_methods(lua_State* L, const luaL_Reg* reg, int index)
{
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
inline int lexnew_metatable(lua_State* L, const char* name, const luaL_Reg* methods)
{
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
inline void* lexnew_userdata(lua_State* L, const char* name, size_t size)
{
  void* userdata = lua_newuserdata(L, size);
  lexset_metatable(L, name);
  return userdata;
}

/*
** Create a new userdata and set metatable
*/
template <typename _Ty>
inline _Ty* lexnew_userdata(lua_State* L, const char* name)
{
  return (_Ty*)lexnew_userdata(L, name, sizeof(_Ty));
}

/*
** Get and check userdata from stack
** if there is any error, lexget_userdata catches it
*/
template <typename _Ty>
inline _Ty* lexget_userdata(lua_State* L, int index, const char* name)
{
  _Ty* userdata = (_Ty*)luaL_checkudata(L, index, name);
  luaL_argcheck(L, userdata != NULL, index, "userdata value expected");
  return userdata;
}

/***********************************************************************************/

inline void lexadd_loader(lua_State* L, lua_CFunction fn)
{
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield(L, -1, "lex_loader");
  if (lua_iscfunction(L, -1)) {
    lua_pop(L, 2);
    return;
  }
  lua_pop(L, 1);
  // set table field "lex_loader"
  lua_pushcfunction(L, fn);
  lua_setfield(L, -2, "lex_loader");
  // push "package"
#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM == 501
  lua_getfield(L, -1, "loaders");
#else
  lua_getfield(L, -1, "searchers");
#endif
  // push "package.loaders"
  lua_remove(L, -2);
  // remove "package"
  lua_pushcfunction(L, fn);
  lua_rawseti(L, -2, 2);
  // Table is still on the stack.  Get rid of it now.
  lua_pop(L, 1);
  // replace "dofile" of lua function
  lua_register(L, "dofile", fn);
}

inline void lex_preload(lua_State* L, const luaL_Reg* libs)
{
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  for (; libs->func; libs++) {
    lua_pushcfunction(L, libs->func);
    lua_setfield(L, -2, libs->name);
  }
  lua_pop(L, 2);
}

inline void lex_preload(lua_State* L, const char* name, lua_CFunction func)
{
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_pushcfunction(L, func);
  lua_setfield(L, -2, name);
  lua_pop(L, 2);
}

inline void lex_protect(lua_State* L, lua_CFunction fn)
{
  lua_getglobal(L, "_G");
  lua_getfield(L, -1, "__newindex");
  if (lua_isfunction(L, -1)) { //已添加过了
    lua_pop(L, 2);
    return;
  }
  lua_pop(L, 1);
  luaL_newmetatable(L, "placeholders");
  lua_pushliteral(L, "__newindex");
  lua_pushcfunction(L, fn);
  lua_rawset(L, -3);
  auto b = lua_gettop(L);

  lua_setmetatable(L, -2);
  lua_pop(L, 1);
}

inline const std::string& strrstr(std::string& str, const std::string& old_value, const std::string& new_value)
{
  std::string::size_type pos = str.find(old_value);
  while (pos != std::string::npos) {
    str.replace(pos, old_value.length(), new_value);
    pos = str.find(old_value, pos + new_value.size());
  }
  return std::move(str);
}

inline std::string str_tolower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); }
  );
  return std::move(s);
}

inline void normalize(std::string& path, std::string& filename)
{
  std::string ext;
  if (filename.size() >= 5) {
    ext = filename.substr(filename.size() - 5);
    ext = str_tolower(ext);
    if (ext.substr(1) == ".lua") {
      filename = filename.substr(0, filename.size() - 4);
      return;
    }
    if (ext == ".luac") {
      filename = filename.substr(0, filename.size() - 5);
    }
  }
#ifdef os_windows
  path = strrstr(path, POSIX_DSCHR, WIN_DSCHR);
  filename = strrstr(filename, ".", WIN_DSCHR);
  filename = strrstr(filename, POSIX_DSCHR, WIN_DSCHR);
  filename = strrstr(filename, "..\\", "");
#else
  path = strrstr(path, WIN_DSCHR, POSIX_DSCHR);
  filename = strrstr(filename, ".", POSIX_DSCHR);
  filename = strrstr(filename, WIN_DSCHR, POSIX_DSCHR);
  filename = strrstr(filename, "../", "");
#endif
}

/*
** Loads and runs the given file, if there is any error, lex_dofile catches it
** ns is number of return values
*/
template <int ns = 0>
inline void lex_dofile(lua_State* L, const char* filename, lua_CFunction loader)
{
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield(L, -1, "lex_loader");
  if (!lua_iscfunction(L, -1))
  {
    lua_pop(L, 1);
    lua_getfield(L, -1, "path");
    std::string path = lexget_string(L, -1);
    lua_pop(L, 2);

    std::string name(filename);
    normalize(path, name);
    path = strrstr(path, "?", name);

    name.clear();
    const char* p = path.c_str();
    bool ok = false;
    while (p[0])
    {
      char c = *p++;
      if (c != ';') {
        name += c;
        continue;
      }
      if (luaL_dofile(L, name.c_str()) == LUA_OK) {
        ok = true;
        break;
      }
      name.clear();
    }
    if (!ok && luaL_dofile(L, name.c_str())) {
      lexpop_error(L, -1);
    }
    return;
  }
  lua_CFunction lex_loader = lua_tocfunction(L, -1);
  if (lex_loader != nullptr) {
    lua_pop(L, 2);
    lua_pushcfunction(L, lex_loader);
    lua_pushstring(L, filename);
    lua_pushboolean(L, 1);
    lua_pushcfunction(L, loader);
    //call lex_loader(L);
    if (lua_pcall(L, 3, -1, 0)) {
      lexpop_error(L, -1);
    }
  }
}

/*
** Calls a function in protected mode, if there is any error, lex_pcall catches it
** ns is number of return values
*/
template <int ns = 0, typename... Args>
inline void lex_pcall(lua_State* L, lua_function fn, Args... args)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, fn);
  lexpush_params(L, args...);
  if (lua_pcall(L, (int)sizeof...(args), ns, 0)) {
    lexpop_error(L, -1);
  }
}

/*
** Calls a function in protected mode, if there is any error, lex_pcall catches it
** ns is number of return values
*/
template <int ns = 0, typename... Args>
inline void lex_pcall(lua_State* L, lua_function fn, int self, Args... args)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, fn);
  if (self > 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, self);
  }
  lexpush_params(L, args...);
  if (lua_pcall(L, (int)sizeof...(args) + (self ? 1 : 0), ns, 0)) {
    lexpop_error(L, -1);
  }
}

/*
** Calls a function in protected mode, if there is any error, lex_pcall catches it
** ns is number of return values
*/
template <int ns = 0, typename... Args>
inline void lex_pcall(lua_State* L, const char* funcname, Args... args)
{
  lua_getglobal(L, funcname);
  if (lua_type(L, -1) != LUA_TFUNCTION) {
    lua_pop(L, 1);
    char errstr[512];
    sprintf(errstr, "function: %s not found", funcname);
    throw lua_exception(errstr);
  }
  lexpush_params(L, args...);
  if (lua_pcall(L, (int)sizeof...(args), ns, 0)) {
    lexpop_error(L, -1);
  }
}

/*
** Calls a function in protected mode, if there is any error, lex_pcall catches it
** ns is number of return values
*/
template <int ns = 0>
inline void lex_pcall(lua_State* L, const char* funcname, const char* modname, const std::vector<std::any>& argvs)
{
  lua_getglobal(L, funcname);
  if (lua_type(L, -1) != LUA_TFUNCTION) {
    lua_pop(L, 1);
    char errstr[512];
    sprintf(errstr, "function: %s not found", funcname);
    throw lua_exception(errstr);
  }
  lua_pushstring(L, modname);
  for (size_t i = 0; i < argvs.size(); i++) {
    lexpush_any(L, argvs[i]);
  }
  if (lua_pcall(L, (int)argvs.size() + 1, ns, 0)) {
    lexpop_error(L, -1);
  }
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
struct lua_CFunction_param<size_t> {
  static size_t get(lua_State* L, int i) {
    return (size_t)luaL_checkinteger(L, i);
  }
  static size_t get(lua_State* L, int i, size_t def) {
    return lua_isnoneornil(L, i) ? def : get(L, i);
  }
  static void set(lua_State* L, size_t v) {
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

template<>
struct lua_CFunction_param<lua_table<>> {
  static lua_table<> get(lua_State* L, int i) {
    luaL_argcheck(L, lua_type(L, i) == LUA_TTABLE, i, "value expected");
    return std::any_cast<lua_table<>>(lexget_any(L, i));
  }
  static lua_table<> get(lua_State* L, int i, const lua_table<>& def) {
    return lua_isnoneornil(L, i) ? std::any_cast<lua_table<>>(def) : get(L, i);
  }
  static void set(lua_State* L, const lua_table<>& v) {
    lexpush_any(L, v);
  }
};

/***********************************************************************************/

template <typename Handler>
struct lua_CFunction_userdata {
  lua_CFunction_userdata(Handler fn) : cfunction(fn) { }
  Handler cfunction;
};

template <typename Handler>
struct lua_CFunction_wrapper
{
  typedef void (*dest_func_t)();
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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
  typedef lua_CFunction_userdata<dest_func_t> lua_CFunction_userdata;
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

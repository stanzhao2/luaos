

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
#include <os/os.h>
#include <wrapper/lex_type.h>

#ifdef __cplusplus
# undef  LUA_API
# define LUA_API extern "C"
#endif

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



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

#include <string>
#include <memory>
#include <mutex>
#include <lua.hpp> 

/***********************************************************************************/

class lua_table final {
  class lua_state final {
    std::mutex _lock;
    lua_State* _L;

  public:
    inline lua_state() : _L(luaL_newstate()) { }
    inline ~lua_state() { lua_close(_L); }
    inline lua_State* L() const { return _L; }
    inline std::mutex& lock()   { return _lock; }
  };
  inline static lua_state& state() {
    static lua_state _state;
    return _state;
  }
  lua_table(lua_State* F, int index)
    : lua_table()
  {
    if (!F || index == 0) {
      return;
    }
    if (!(L = state().L())) {
      return;
    }
    std::unique_lock<std::mutex> lock(
      state().lock()
    );
    if (lua_type(F, index) == LUA_TNONE) {
      lua_newtable(L);
      return;
    }
    clone(F, L, index, 0);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  int ref;
  lua_State* L;

public:
  typedef std::shared_ptr<lua_table> value_type;
  inline lua_table()
    : ref(0), L(nullptr) {
  }
  virtual ~lua_table() {
    if (!L || !ref) {
      return;
    }
    std::unique_lock<std::mutex> lock(
      state().lock()
    );
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
  }
  void push(lua_State* F) const
  {
    if (!L || !ref) {
      lua_pushnil(F);
      return;
    }
    std::unique_lock<std::mutex> lock(
      state().lock()
    );
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    clone(L, F, lua_gettop(L), 0);
  }
  inline static value_type create() {
    return value_type(new lua_table());
  }
  inline static value_type create(lua_State* F, int index) {
    return value_type(new lua_table(F, index));
  }
  static void clone(lua_State* F, lua_State* T, int index, int level)
  {
    if (level > 32) {
      luaL_error(F, "table nesting level is too deep");
    }
    if (index < 0) {
      index = lua_gettop(F) + index + 1;
    }
    lua_newtable(T);
    lua_pushnil(F);
    while (lua_next(F, index))
    {
      size_t size = 0;
      int key_type = lua_type(F, -2);
      switch (key_type) {
      case LUA_TTABLE:
        clone(F, T, -2, level++);
        break;
      case LUA_TNUMBER:
        lua_pushinteger(T, lua_tointeger(F, -2));
        break;
      case LUA_TSTRING:
        lua_pushlstring(T, lua_tolstring(F, -2, &size), size);
        break;
      default:
        lua_pushstring(T, lua_tostring(F, -2));
        break;
      }
      int value_type = lua_type(F, -1);
      switch (value_type) {
      case LUA_TTABLE:
        clone(F, T, -1, level++);
        break;
      case LUA_TBOOLEAN:
        lua_pushboolean(T, lua_toboolean(F, -1));
        break;
      case LUA_TUSERDATA:
      case LUA_TLIGHTUSERDATA:
        lua_pushlightuserdata(T, lua_touserdata(F, -1));
        break;
      case LUA_TSTRING:
        lua_pushlstring(T, lua_tolstring(F, -1, &size), size);
        break;
      case LUA_TNUMBER:
        if (lua_isinteger(F, -1))
          lua_pushinteger(T, luaL_checkinteger(F, -1));
        else
          lua_pushnumber(T, lua_tonumber(F, -1));
        break;
      default:
        lua_pushstring(T, lua_tostring(F, -1));
        break;
      }
      lua_settable(T, -3);
      lua_pop(F, 1);
      lua_checkstack(T, 3);
    }
  }
};

/***********************************************************************************/

class lua_function {
  int r;

public:
  inline lua_function(int n = 0)
    : r(n) {
  }
  inline lua_function& operator=(int n) {
    return (r = n), *this;
  }
  inline operator int() const {
    return r;
  }
  inline lua_function& ref(lua_State* L, int index) {
    lua_pushvalue(L, index);
    r = luaL_ref(L, LUA_REGISTRYINDEX);
    return *this;
  }
  inline lua_function& unref(lua_State* L) {
    if (r) r = (luaL_unref(L, LUA_REGISTRYINDEX, r), 0);
    return *this;
  }
  inline lua_function& rawget(lua_State* L) {
    if (r) lua_rawgeti(L, LUA_REGISTRYINDEX, r);
    return *this;
  }
};

/***********************************************************************************/

class lua_value final {
  struct {
    const void*  _ud = nullptr;
    int          _vb = 0;
    lua_Integer  _vi = 0;
    lua_Number   _vf = 0;
    std::string  _vstr;
    lua_function _vfn;
    lua_table::value_type _vtbl;
  } _data;
  int  _type;
  bool _integer;

public:
  inline ~lua_value() {

  }
  inline bool has_value() const {
    return (_type != LUA_TNIL);
  }
  inline int type() const {
    return _type;
  }
  inline bool is_integer() const {
    return _integer;
  }
  inline lua_value()
    : _type(LUA_TNIL), _integer(false) {
  }
  inline lua_value(bool v) {
    *this = v;
  }
  inline lua_value(lua_function v) {
    *this = v;
  }
  inline lua_value(lua_Integer v) {
    *this = v;
  }
  inline lua_value(lua_Number v) {
    *this = v;
  }
  inline lua_value(void* v) {
    *this = v;
  }
  inline lua_value(const void* v) {
    *this = v;
  }
  inline lua_value(char* v) {
    *this = v;
  }
  inline lua_value(const char* v) {
    *this = v;
  }
  inline lua_value(const std::string& v) {
    *this = v;
  }
  inline lua_value(lua_table::value_type v) {
    *this = v;
  }
  inline lua_value(const lua_value& v)  {
    *this = v;
  }
  inline void operator=(const lua_value& v) {
    _type    = v._type;
    _integer = v._integer;
    _data    = v._data;
  }
  inline void operator=(bool v) {
    _type = LUA_TBOOLEAN;
    _data._vb = v ? 1 : 0;
  }
  inline void operator=(lua_function v) {
    _type = LUA_TFUNCTION;
    _data._vfn = v;
  }
  inline void operator=(lua_Integer v) {
    _integer = true;
    _type = LUA_TNUMBER;
    _data._vi = v;
  }
  inline void operator=(lua_Number v) {
    _type = LUA_TNUMBER;
    _data._vf = v;
  }
  inline void operator=(char* v) {
    _type = LUA_TSTRING;
    _data._vstr = v;
  }
  inline void operator=(const char* v) {
    _type = LUA_TSTRING;
    _data._vstr = v;
  }
  inline void operator=(const std::string& v) {
    _type = LUA_TSTRING;
    _data._vstr = v;
  }
  inline void operator=(void* v) {
    _type = LUA_TUSERDATA;
    _data._ud = v;
  }
  inline void operator=(const void* v) {
    _type = LUA_TUSERDATA;
    _data._ud = v;
  }
  inline void operator=(lua_table::value_type v) {
    _type = LUA_TTABLE;
    _data._vtbl = v;
  }
  inline operator bool() const {
    return _data._vb ? true : false;
  }
  inline operator lua_function() const {
    return _data._vfn;
  }
  inline operator const lua_function&() const {
    return _data._vfn;
  }
  inline operator lua_Integer() const {
    return _data._vi;
  }
  inline operator lua_Number() const {
    return _data._vf;
  }
  inline operator void*() const {
    return (void*)_data._ud;
  }
  inline operator const void*() const {
    return _data._ud;
  }
  inline operator char*() const {
    return (char*)_data._vstr.c_str();
  }
  inline operator const char*() const {
    return _data._vstr.c_str();
  }
  inline operator std::string() const {
    return _data._vstr;
  }
  inline operator lua_table::value_type() const {
    return _data._vtbl;
  }
};

/***********************************************************************************/

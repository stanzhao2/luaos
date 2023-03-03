

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
    inline lua_state()
      : _L(luaL_newstate()) {
    }
    inline ~lua_state() {
      lua_close(_L);
    }
    inline lua_State* L() const {
      return _L;
    }
    inline std::mutex& lock() {
      return _lock;
    }
  };
  inline static std::shared_ptr<lua_state> state() {
    static std::shared_ptr<lua_state> _state(new lua_state());
    return _state;
  }
  inline lua_table()
    : ref(0), L(nullptr) {
  }
  lua_table(lua_State* F, int index)
    : lua_table()
  {
    st = state();
    if (!F || index == 0) {
      return;
    }
    if (!(L = st->L())) {
      return;
    }
    std::unique_lock<std::mutex> lock(st->lock());
    if (lua_type(F, index) == LUA_TNONE) {
      lua_newtable(L);
      return;
    }
    clone(F, L, index, 0);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  int ref;
  lua_State* L;
  std::shared_ptr<lua_state> st;

public:
  typedef std::shared_ptr<lua_table> value_type;

  virtual ~lua_table() {
    if (!L || !ref) {
      return;
    }
    std::unique_lock<std::mutex> lock(st->lock());
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
  }
  void push(lua_State* F) const
  {
    if (!L || !ref) {
      lua_pushnil(F);
      return;
    }
    std::unique_lock<std::mutex> lock(st->lock());
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    clone(L, F, lua_gettop(L), 0);
  }
  inline static value_type create()
  {
    return value_type(new lua_table());
  }
  inline static value_type create(lua_State* F, int index)
  {
    return value_type(new lua_table(F, index));
  }
  static void push(lua_State* F, lua_State* T, int index)
  {
    if (lua_isinteger(F, -1)) {
      lua_pushinteger(T, lua_tointeger(F, -1));
      return;
    }
    lua_pushnumber(T, lua_tonumber(F, -1));
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
      switch (lua_type(F, -2)) {
      case LUA_TTABLE:
        clone(F, T, -2, level + 1);
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
      switch (lua_type(F, -1)) {
      case LUA_TTABLE:
        clone(F, T, -1, level + 1);
        break;

      case LUA_TNUMBER:
        push(F, T, index);
        break;

      case LUA_TBOOLEAN:
        lua_pushboolean(T, lua_toboolean(F, -1));
        break;

      case LUA_TUSERDATA:
        lua_pushlightuserdata(T, lua_touserdata(F, -1));
        break;

      case LUA_TLIGHTUSERDATA:
        lua_pushlightuserdata(T, lua_touserdata(F, -1));
        break;

      case LUA_TSTRING:
        lua_pushlstring(T, lua_tolstring(F, -1, &size), size);
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

class lua_method final {
  inline lua_method(int v)
    : r(v), L(nullptr) {
  }
  inline lua_method(lua_State* L, int index)
  {
    this->L = L;
    lua_pushvalue(L, index);
    r = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  int r;
  lua_State* L;

public:
  typedef std::shared_ptr<lua_method> value_type;

  inline static value_type create(int v = 0) {
    return value_type(new lua_method(v));
  }
  inline static value_type create(lua_State* L, int index) {
    return value_type(new lua_method(L, index));
  }
  inline ~lua_method() {
    if (L && r) {
      luaL_unref(L, LUA_REGISTRYINDEX, r);
    }
  }
  inline void rawget(lua_State* L) const {
    lua_rawgeti(L, LUA_REGISTRYINDEX, r);
  }
};

/***********************************************************************************/

enum struct lua_ctype {
  nil                 = 1,
  none                = 2,
  boolean             = 3,
  integer             = 4,
  number              = 5,
  string              = 6,
  table               = 7,
  userdata            = 8,
  function            = 9
};

class lua_value final {
  struct {
    const void*   _ud = nullptr;
    int           _vb = 0;
    lua_Integer   _vi = 0;
    lua_Number    _vf = 0;
    std::string   _vstr;
    lua_table::value_type  _vtbl;
    lua_method::value_type _vfn;
  } _data;
  lua_ctype _type;

  inline void check_number(lua_State* L, int i)
  {
    if (lua_isinteger(L, i)) {
      *this = luaL_checkinteger(L, i);
      return;
    }
    *this = luaL_checknumber(L, i);
  }

public:
  inline bool has_value() const {
    return (_type != lua_ctype::nil);
  }
  inline lua_ctype type() const {
    return _type;
  }
  inline lua_value()
    : _type(lua_ctype::nil) {
  }
  inline lua_value(lua_State* L, int i) {
    assign(L, i);
  }
  inline lua_value(const lua_value& v) {
    *this = v;
  }
  inline lua_value(bool v) {
    *this = v;
  }
  inline lua_value(char v) {
    *this = v;
  }
  inline lua_value(unsigned char v) {
    *this = v;
  }
  inline lua_value(short v) {
    *this = v;
  }
  inline lua_value(unsigned short v) {
    *this = v;
  }
  inline lua_value(int v) {
    *this = v;
  }
  inline lua_value(unsigned int v) {
    *this = v;
  }
  inline lua_value(long v) {
    *this = v;
  }
  inline lua_value(lua_Integer v) {
    *this = v;
  }
  inline lua_value(size_t v) {
    *this = v;
  }
  inline lua_value(float v) {
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
  inline lua_value(lua_method::value_type v) {
    *this = v;
  }
  inline void operator=(const lua_value& v) {
    copy(v);
  }
  inline void operator=(bool v) {
    _type = lua_ctype::boolean;
    _data._vb = v ? 1 : 0;
  }
  inline void operator=(char v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(unsigned char v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(short v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(unsigned short v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(int v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(unsigned int v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(long v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(lua_Integer v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(size_t v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(float v) {
    _type = lua_ctype::integer;
    _data._vf = v;
  }
  inline void operator=(lua_Number v) {
    _type = lua_ctype::number;
    _data._vf = v;
  }
  inline void operator=(char* v) {
    _type = lua_ctype::string;
    _data._vstr = v;
  }
  inline void operator=(const char* v) {
    _type = lua_ctype::string;
    _data._vstr = v;
  }
  inline void operator=(const std::string& v) {
    _type = lua_ctype::string;
    _data._vstr = v;
  }
  inline void assign(const char* data, size_t size) {
    _type = lua_ctype::string;
    _data._vstr.assign(data, size);
  }
  inline void operator=(void* v) {
    _type = lua_ctype::userdata;
    _data._ud = v;
  }
  inline void operator=(const void* v) {
    _type = lua_ctype::userdata;
    _data._ud = v;
  }
  inline void operator=(lua_table::value_type v) {
    _type = lua_ctype::table;
    _data._vtbl = v;
  }
  inline void operator=(lua_method::value_type v) {
    _type = lua_ctype::function;
    _data._vfn = v;
  }
  inline operator bool() const {
    return _data._vb ? true : false;
  }
  inline operator char() const {
    return (char)_data._vi;
  }
  inline operator unsigned char() const {
    return (unsigned char)_data._vi;
  }
  inline operator short() const {
    return (short)_data._vi;
  }
  inline operator unsigned short() const {
    return (unsigned short)_data._vi;
  }
  inline operator int() const {
    return (int)_data._vi;
  }
  inline operator unsigned int() const {
    return (unsigned int)_data._vi;
  }
  inline operator long() const {
    return (long)_data._vi;
  }
  inline operator lua_Integer() const {
    return (lua_Integer)_data._vi;
  }
  inline operator size_t() const {
    return (size_t)_data._vi;
  }
  inline operator float() const {
    return (float)_data._vf;
  }
  inline operator lua_Number() const {
    return (lua_Number)_data._vf;
  }
  inline operator void* () const {
    return (void*)_data._ud;
  }
  inline operator const void* () const {
    return _data._ud;
  }
  inline operator char* () const {
    return (char*)_data._vstr.c_str();
  }
  inline operator const char* () const {
    return _data._vstr.c_str();
  }
  inline operator std::string() const {
    return _data._vstr;
  }
  inline operator lua_table::value_type() const {
    return _data._vtbl;
  }
  inline operator lua_method::value_type() const {
    return _data._vfn;
  }
  inline void copy(const lua_value& r)
  {
    switch (r.type()) {
    case lua_ctype::integer:
      _data._vi   = r._data._vi;
      break;

    case lua_ctype::number:
      _data._vf   = r._data._vf;
      break;

    case lua_ctype::string:
      _data._vstr = r._data._vstr;
      break;

    case lua_ctype::table:
      _data._vtbl = r._data._vtbl;
      break;

    case lua_ctype::boolean:
      _data._vb   = r._data._vb;
      break;

    case lua_ctype::function:
      _data._vfn  = r._data._vfn;
      break;

    case lua_ctype::userdata:
      _data._ud   = r._data._ud;
      break;
    }
    _type = r._type;
  }
  inline void push(lua_State* L) const
  {
    switch (type()) {
    case lua_ctype::integer:
      lua_pushinteger(L, _data._vi);
      return;

    case lua_ctype::number:
      lua_pushnumber(L, _data._vf);
      return;

    case lua_ctype::string:
      lua_pushlstring(L, _data._vstr.c_str(), _data._vstr.size());
      return;

    case lua_ctype::table:
      _data._vtbl->push(L);
      return;

    case lua_ctype::boolean:
      lua_pushboolean(L, _data._vb);
      return;

    case lua_ctype::function:
      _data._vfn->rawget(L);
      return;

    case lua_ctype::userdata:
      lua_pushlightuserdata(L, (void*)_data._ud);
      return;
    }
    lua_pushnil(L);
  }
  inline void assign(lua_State* L, int index)
  {
    _type = lua_ctype::nil;
    if (index < 0) {
      index = lua_gettop(L) + 1 + index;
    }
    if (lua_isnoneornil(L, index)) {
      return;
    }
    switch (lua_type(L, index))
    {
      case LUA_TNUMBER: {
        check_number(L, index);
        return;
      }
      case LUA_TTABLE: {
        *this = lua_table::create(L, index);
        return;
      }
      case LUA_TSTRING: {
        size_t size = 0;
        const char* data = luaL_checklstring(L, index, &size);
        assign(data, size);
        return;
      }
      case LUA_TBOOLEAN: {
        *this = lua_toboolean(L, index) ? true : false;
        return;
      }
      case LUA_TUSERDATA: {
        *this = lua_touserdata(L, index);
        return;
      }
      case LUA_TFUNCTION: {
        *this = lua_method::create(L, index);
        return;
      }
    }
  }
};

/***********************************************************************************/

template <typename... Args>
inline void luaL_pushvalues(lua_State* L, Args... args) {
  lua_value params[] = { args... };
  for (size_t i = 0; i < sizeof...(args); i++) {
    params[i].push(L);
  }
}

/***********************************************************************************/

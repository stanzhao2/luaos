

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

#include "luaos_pack.h"

/***********************************************************************************/

int lua_packany(
  lua_State* L, int index, std::string& out
);

int lua_unpackany(
  lua_State* L, const std::string& data
);

/***********************************************************************************/

class lua_table final {
  std::string _data;

public:
  inline lua_table()
    : _data("") {  
  }
  inline lua_table(const lua_table& r)
    : _data(r._data) {
  }
  inline lua_table(lua_State* L, int index) {
    lua_packany(L, index, _data);
  }
  inline void push(lua_State* L) const {
    lua_unpackany(L, _data);
  }
};

/***********************************************************************************/

class lua_method final {
  int r;
  lua_State* L;

public:
  inline lua_method(int v = 0)
    : r(v), L(nullptr) {
  }
  inline lua_method(const lua_method& v)
    :r(v.r), L(v.L){    
  }
  inline lua_method(lua_State* L, int index) {
    this->L = L;
    lua_pushvalue(L, index);
    r = luaL_ref(L, LUA_REGISTRYINDEX);
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
  boolean             = 2,
  integer             = 3,
  number              = 4,
  string              = 5,
  table               = 6,
  userdata            = 7,
  function            = 8
};

class lua_value final {
  struct {
    const void*   _ud = nullptr;
    int           _vb = 0;
    lua_Integer   _vi = 0;
    lua_Number    _vf = 0;
    std::string   _vstr;
    lua_table     _vtbl;
    lua_method    _vfn;
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
  inline lua_value(lua_Integer v) {
    *this = v;
  }
  inline lua_value(size_t v) {
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
  inline lua_value(lua_table v) {
    *this = v;
  }
  inline lua_value(lua_method v) {
    *this = v;
  }
  inline void operator=(const lua_value& v) {
    copy(v);
  }
  inline void operator=(bool v) {
    _type = lua_ctype::boolean;
    _data._vb = v ? 1 : 0;
  }
  inline void operator=(lua_Integer v) {
    _type = lua_ctype::integer;
    _data._vi = v;
  }
  inline void operator=(size_t v) {
    _type = lua_ctype::integer;
    _data._vi = v;
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
  inline void operator=(void* v) {
    _type = lua_ctype::userdata;
    _data._ud = v;
  }
  inline void operator=(const void* v) {
    _type = lua_ctype::userdata;
    _data._ud = v;
  }
  inline void operator=(lua_table v) {
    _type = lua_ctype::table;
    _data._vtbl = v;
  }
  inline void operator=(lua_method v) {
    _type = lua_ctype::function;
    _data._vfn = v;
  }
  inline void assign(const char* data, size_t size) {
    _type = lua_ctype::string;
    _data._vstr.assign(data, size);
  }
  inline operator bool() const {
    return _data._vb ? true : false;
  }
  inline operator lua_Integer() const {
    return (lua_Integer)_data._vi;
  }
  inline operator size_t() const {
    return (size_t)_data._vi;
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
  inline operator lua_table() const {
    return _data._vtbl;
  }
  inline operator lua_method() const {
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

    case lua_ctype::table:
      _data._vtbl.push(L);
      return;

    case lua_ctype::boolean:
      lua_pushboolean(L, _data._vb);
      return;

    case lua_ctype::function:
      _data._vfn.rawget(L);
      return;

    case lua_ctype::string:
      lua_pushlstring(L, _data._vstr.c_str(), _data._vstr.size());
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
        *this = lua_table(L, index);
        return;
      }
      case LUA_TFUNCTION: {
        *this = lua_method(L, index);
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
      case LUA_TSTRING: {
        size_t size = 0;
        const char* data = luaL_checklstring(L, index, &size);
        assign(data, size);
        return;
      }
    }
  }
};

/***********************************************************************************/

class lua_value_array final {
  inline lua_value_array(lua_State* L, int begin, int end) {
    append(L, begin, end);
  }
  inline lua_value_array() {}
  std::vector<lua_value> _values;

public:
  typedef std::shared_ptr<lua_value_array> value_type;

  inline size_t push(lua_State* L) const {
    for (size_t i = 0; i < _values.size(); i++)
      _values[i].push(L);
    return size();
  }
  inline size_t size() const {
    return _values.size();
  }
  inline void append(lua_State* L, lua_value v) {
    _values.push_back(v);
  }
  inline void append(lua_State* L, int begin, int end) {
    for (int i = begin; i < end; i++) {
      _values.push_back(lua_value(L, i));
    }
  }
  inline static value_type create(lua_State* L, int begin, int end = 0) {
    return value_type(new lua_value_array(L, begin, end ? end : lua_gettop(L)));
  }
  inline static value_type create() {
    return value_type(new lua_value_array());
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

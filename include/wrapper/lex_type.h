

/***********************************************************************************/

#pragma once

#include <string.h>
#include <string>
#include <map>
#include <memory>
#include <lua.hpp> 

#include "lex_table.h"

/***********************************************************************************/

struct lua_function {
    inline lua_function(int n = 0)
        : refn(n) {
    }
    inline lua_function& operator=(int n) {
        return (refn = n), *this;
    }
    inline operator int() const {
        return refn;
    }
    inline void unref(lua_State* L) {
        luaL_unref(L, LUA_REGISTRYINDEX, refn), refn = 0;
    }
private:
    int refn;
};

/***********************************************************************************/

enum struct c_type {
    i_unknow       = 0,
    i_void_ptr     ,
    i_bool         ,
    i_char         ,
    i_short        ,
    i_int          ,
    i_long         ,
    i_float        ,
    i_double       ,
    i_longlong     ,
    i_uchar        ,
    i_ushort       ,
    i_uint         ,
    i_ulonglong    ,
    i_size_t       ,
    i_string       ,
    i_lua_number   ,
    i_lua_integer  ,
    i_lua_table    ,
    i_lua_function 
};

/***********************************************************************************/

const size_t void_hash_code                 =   typeid(void*).hash_code();
const size_t bool_hash_code                 =   typeid(bool).hash_code();
const size_t char_hash_code                 =   typeid(char).hash_code();
const size_t short_hash_code                =   typeid(short).hash_code();
const size_t int_hash_code                  =   typeid(int).hash_code();
const size_t long_hash_code                 =   typeid(long).hash_code();
const size_t float_hash_code                =   typeid(float).hash_code();
const size_t double_hash_code               =   typeid(double).hash_code();
const size_t long_long_hash_code            =   typeid(long long).hash_code();
const size_t unsigned_char_hash_code        =   typeid(unsigned char).hash_code();
const size_t unsigned_short_hash_code       =   typeid(unsigned short).hash_code();
const size_t unsigned_int_hash_code         =   typeid(unsigned int).hash_code();
const size_t unsigned_long_long_hash_code   =   typeid(unsigned long long).hash_code();
const size_t i_size_t_hash_code             =   typeid(size_t).hash_code();
const size_t std_string_hash_code           =   typeid(std::string).hash_code();
const size_t lua_Number_hash_code           =   typeid(lua_Number).hash_code();
const size_t lua_Integer_hash_code          =   typeid(lua_Integer).hash_code();
const size_t lua_table_hash_code            =   typeid(lua_table<>).hash_code();
const size_t lua_function_hash_code         =   typeid(lua_function).hash_code();

/***********************************************************************************/

inline c_type type_id(const std::type_info& v)
{
    const auto vcode = v.hash_code();
    if (vcode == std_string_hash_code) {
        return c_type::i_string;
    }
    if (vcode == lua_Number_hash_code) {
        return c_type::i_lua_number;
    }
    if (vcode == lua_Integer_hash_code) {
        return c_type::i_lua_integer;
    }
    if (vcode == lua_table_hash_code) {
        return c_type::i_lua_table;
    }
    if (vcode == lua_function_hash_code) {
        return c_type::i_lua_function;
    }
    if (vcode == i_size_t_hash_code) {
        return c_type::i_size_t;
    }
    if (vcode == unsigned_long_long_hash_code) {
        return c_type::i_ulonglong;
    }
    if (vcode == void_hash_code) {
        return c_type::i_void_ptr;
    }
    if (vcode == bool_hash_code) {
        return c_type::i_bool;
    }
    if (vcode == char_hash_code) {
        return c_type::i_char;
    }
    if (vcode == short_hash_code) {
        return c_type::i_short;
    }
    if (vcode == int_hash_code) {
        return c_type::i_int;
    }
    if (vcode == float_hash_code) {
        return c_type::i_float;
    }
    if (vcode == double_hash_code) {
        return c_type::i_double;
    }
    if (vcode == long_hash_code) {
        return c_type::i_long;
    }
    if (vcode == long_long_hash_code) {
        return c_type::i_longlong;
    }
    if (vcode == unsigned_char_hash_code) {
        return c_type::i_uchar;
    }
    if (vcode == unsigned_short_hash_code) {
        return c_type::i_ushort;
    }
    if (vcode == unsigned_int_hash_code) {
        return c_type::i_uint;
    }
    return c_type::i_unknow;
#if 0
    static thread_local std::map<size_t, c_type> _map;
    if (_map.empty())
    {
        _map[typeid(void*).hash_code()]              = c_type::i_void_ptr    ;
        _map[typeid(bool).hash_code()]               = c_type::i_bool        ;
        _map[typeid(char).hash_code()]               = c_type::i_char        ;
        _map[typeid(short).hash_code()]              = c_type::i_short       ;
        _map[typeid(int).hash_code()]                = c_type::i_int         ;
        _map[typeid(float).hash_code()]              = c_type::i_float       ;
        _map[typeid(double).hash_code()]             = c_type::i_double      ;
        _map[typeid(long).hash_code()]               = c_type::i_long        ;
        _map[typeid(long long).hash_code()]          = c_type::i_longlong    ;
        _map[typeid(unsigned char).hash_code()]      = c_type::i_uchar       ;
        _map[typeid(unsigned short).hash_code()]     = c_type::i_ushort      ;
        _map[typeid(unsigned int).hash_code()]       = c_type::i_uint        ;
        _map[typeid(long unsigned int).hash_code()]  = c_type::i_uint_long   ;
        _map[typeid(unsigned long long).hash_code()] = c_type::i_ulonglong   ;
        _map[typeid(std::string).hash_code()]        = c_type::i_string      ;
        _map[typeid(lua_Number).hash_code()]         = c_type::i_lua_number  ;
        _map[typeid(lua_Integer).hash_code()]        = c_type::i_lua_integer ;
        _map[typeid(lua_table<>).hash_code()]        = c_type::i_lua_table   ;
        _map[typeid(lua_function).hash_code()]       = c_type::i_lua_function;
    }
    auto iter = _map.find(v.hash_code());
    return iter != _map.end() ? iter->second : c_type::i_unknow;
#endif
}

/***********************************************************************************/

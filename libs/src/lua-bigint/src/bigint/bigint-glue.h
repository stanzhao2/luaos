
#pragma once

#include <lua_wrapper.h>

int bigint_new(lua_State *L);
int bigint_destroy(lua_State *L);
int bigint_tostring(lua_State *L);
int bigint_concat(lua_State *L);
int bigint_add(lua_State *L);
int bigint_sub(lua_State *L);
int bigint_mul(lua_State *L);
int bigint_div(lua_State *L);
int bigint_mod(lua_State *L);
int bigint_pow(lua_State *L);
int bigint_and(lua_State *L);
int bigint_or(lua_State *L);
int bigint_xor(lua_State *L);
int bigint_negate(lua_State *L);
int bigint_equal(lua_State *L);
int bigint_lt(lua_State *L);
int bigint_le(lua_State *L);
int bigint_lshift(lua_State *L);
int bigint_rshift(lua_State *L);
int bigint_gcd(lua_State* L);

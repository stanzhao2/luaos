/* 
 * bigint-glue.cpp
 *
 * Glue code, providing access to the BigInt C++ class from Lua.
 *
 * Copyright (c) 2015 Jorj Bauer
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bigint.h"
#include "bigint-glue.h"
#include "../lua-bigint.h"

static bool _isBigInt(lua_State *L, int index)
{
  if (lua_type(L, index) != LUA_TUSERDATA)
    return 0;
  BigInt **b = (BigInt **)luaL_checkudata(L, index, BIGINT_NAME);
  if (b == NULL || *b == NULL)
    return 0;

  return 1;
}

static BigInt *_checkBigInt(lua_State *L, int index)
{
  // Check that the thing at stack index 'index' is a BIGINT_POINTER.
  luaL_checktype(L, index, LUA_TUSERDATA);
  BigInt **b = (BigInt **)luaL_checkudata(L, index, BIGINT_NAME);
  if (b == NULL || *b == NULL) {
    lua_pushstring(L, "Type error: BigInt data is NULL");
    lua_error(L);
  }
  return *b;
}

void construct_bigint(lua_State *L, int argidx)
{
  BigInt **b = (BigInt **)lua_newuserdata(L, sizeof(BigInt *));

  if (argidx < 0) 
    argidx--; // we just pushed a new element on the stack...

  try {
    switch (lua_type(L, argidx)) {
    case LUA_TNUMBER:
      *b = new BigInt((unsigned long long)luaL_checkinteger(L, argidx));
      break;
    case LUA_TSTRING:
      *b = new BigInt(lua_tostring(L, argidx));
      break;
    case LUA_TUSERDATA:
    {
      // could be another bigint!
      BigInt **b2 = (BigInt **)luaL_checkudata(L, argidx, BIGINT_NAME);
      if (b2 && *b2) {
        *b = new BigInt(**b2);
        break;
      }
      // else fall through
    }
    default:
      luaL_error(L, "value to BigInt.new must be a number, string, or another BigInt");
      return;
    }
  }
  catch (const std::exception& e) {
    luaL_error(L, e.what());
  }
  // Set the metatable for the userdata, so that it's GC'd properly
  luaL_getmetatable(L, BIGINT_NAME);
  lua_setmetatable(L, -2);             
}

int bigint_new(lua_State *L)
{
  switch (lua_gettop(L))
  {
  case 1:
      // allocate memory for a pointer to a C++ object
      construct_bigint(L, 1);
      break;
  case 2:
      // allocate memory for a pointer to a C++ object
      construct_bigint(L, 2);
      break;
  default:
      luaL_error(L, "new requires one or two arguments");
      break;
  }
  return 1; // return the BIGINT_NAME userdata
}

BigInt *_getnum(lua_State *L, int index)
{
  BigInt *b;
  if (_isBigInt(L, index)) {
    b = _checkBigInt(L, index);
  } else {
    // Create a new temporary bigint from the stringified version of stack element #1
    construct_bigint(L, index);
    b = _checkBigInt(L, -1);
  }
  return b;
}

int bigint_destroy(lua_State *L)
{
  BigInt *b = _checkBigInt(L, 1);
  delete b;
  return 0;
}

int bigint_tostring(lua_State *L)
{
  BigInt *b1 = _checkBigInt(L, 1);
  lua_pushstring(L, to_string(*b1).c_str());
  return 1;
}

void _pushStringify(lua_State *L, int idx)
{
  if (!luaL_callmeta(L, idx, "__tostring")) {
    // Didn't have a __tostring metatable
    switch (lua_type(L, idx)) {
    case LUA_TNUMBER:
    case LUA_TSTRING:
      lua_pushvalue(L, idx);
      break;
    case LUA_TBOOLEAN:
      lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
      break;
    case LUA_TNIL:
      lua_pushliteral(L, "nil");
      break;
    default:
      lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
		      lua_topointer(L, idx));
      break;
    }
  }
}

int bigint_concat(lua_State *L)
{
  /* Push a copy of stack element 1 - where the copy is absolutely a string */
  _pushStringify(L, 1);
  /* Repeat on second object to concat */
  _pushStringify(L, 2);

  lua_concat(L, 2);
  return 1;
}

int bigint_add(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "add requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *sum = _checkBigInt(L, -1);

  *sum = *b1 + *b2;
  return 1;
}

int bigint_sub(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "sub requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 - *b2;
  return 1;
}

int bigint_mul(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "mul requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 * *b2;
  return 1;
}

int bigint_div(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "div requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);
  if (b2->zero()) {
    lua_pushstring(L, "division by zero");
    lua_error(L);
    return 0;
  }
  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 / *b2;
  return 1;
}

int bigint_mod(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "mod requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);
  if (b2->zero()) {
    lua_pushstring(L, "division by zero");
    lua_error(L);
    return 0;
  }

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 % *b2;
  return 1;
}

int bigint_pow(lua_State* L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "mod requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);
  if (*b2 < 0) {
    lua_pushstring(L, "#2 requires >= 0");
    lua_error(L);
    return 0;
  }
  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = pow(*b1, *b2);
  return 1;
}

int bigint_negate(lua_State *L)
{
  // I'm not sure why __unm is called with two arguments, but it is.
  if (lua_gettop(L) != 1 && lua_gettop(L) != 2) {
    lua_pushstring(L, "negation requires one argument");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = -(*b1);
  return 1;
}

int bigint_equal(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "equal requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  if (*b1 == *b2) {
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

int bigint_lt(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "lt requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  if (*b1 < *b2) {
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

int bigint_le(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "le requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  if (*b1 <= *b2) {
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

int bigint_lshift(lua_State* L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "left shift requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = (*b1) << (unsigned int)luaL_checkinteger(L, 2);
  return 1;
}

int bigint_rshift(lua_State* L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "right shift requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = (*b1) >> (unsigned int)luaL_checkinteger(L, 2);
  return 1;
}

int bigint_and(lua_State* L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "and requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 & *b2;
  return 1;
}

int bigint_or(lua_State* L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "or requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 | *b2;
  return 1;
}

int bigint_xor(lua_State* L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "xor requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = *b1 ^ *b2;
  return 1;
}

int bigint_gcd(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "gcd requires two arguments");
    lua_error(L);
    return 0;
  }

  BigInt *b1 = _getnum(L, 1);
  BigInt *b2 = _getnum(L, 2);

  lua_pushliteral(L, "0");
  construct_bigint(L, -1);

  BigInt *ret = _checkBigInt(L, -1);
  *ret = gcd(*b1, *b2);
  return 1;
}

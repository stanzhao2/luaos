/*
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

#include "lua-bigint.h"
#include "bigint/bigint-glue.h"

/***********************************************************************************/

static int bigint_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
  { "__gc",         bigint_destroy        },
  { "__tostring",   bigint_tostring       },
  { "__concat",     bigint_concat         },
  { "__add",        bigint_add            },
  { "__sub",        bigint_sub            },
  { "__mul",        bigint_mul            },
  { "__div",        bigint_div            },
  { "__idiv",       bigint_div            },
  { "__mod",        bigint_mod            },
  { "__pow",        bigint_pow            },
  { "__unm",        bigint_negate         },
  { "__band",       bigint_and            },
  { "__bor",        bigint_or             },
  { "__bxor",       bigint_xor            },
  { "__shl",        bigint_lshift         },
  { "__shr",        bigint_rshift         },
  { "__eq",         bigint_equal          },
  { "__lt",         bigint_lt             },
  { "__le",         bigint_le             },
  { "tostring",     bigint_tostring       },
  { NULL,           NULL                  }
  };
  lexnew_metatable(L, BIGINT_NAME, methods);
  lua_pop(L, 1);
  return 0;
}

/***********************************************************************************/

/* Module initializer, called from Lua when the module is loaded. */
LUA_API int luaopen_bigint(lua_State* L)
{
  bigint_metatable(L);
  struct luaL_Reg methods[] = {
    { "new",          bigint_new            },
    { "tostring",     bigint_tostring       },
    { "gcd",          bigint_gcd            },
    { NULL,           NULL                  },
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  return 1;
}

/***********************************************************************************/

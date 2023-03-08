

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

#include <lua_wrapper.h>
#include "random.h"
#include "genrand/genrand.h"

/***********************************************************************************/

typedef struct { void* gen; } random32;

typedef struct { void* gen; } random64;

static random32* check_random32(lua_State* L)
{
  random32* self = lexget_userdata<random32>(L, 1, LIB_RANDOM32NAME);
  if (!self || !self->gen) {
    return nullptr;
  }
  return self;
}

static random64* check_random64(lua_State* L)
{
  random64* self = lexget_userdata<random64>(L, 1, LIB_RANDOM64NAME);
  if (!self || !self->gen) {
    return nullptr;
  }
  return self;
}

/***********************************************************************************/

static int random32_create(lua_State* L)
{
  random32* userdata = lexnew_userdata<random32>(L, LIB_RANDOM32NAME);
  userdata->gen = MT19937_state32();
  return 1;
}

static int random32_srand(lua_State* L)
{
  random32* self = check_random32(L);
  if (self) {
    MT19937_srand32(self->gen, (unsigned long)luaL_checkinteger(L, 2));
  }
  return 0;
}

static int random32_generate(lua_State* L)
{
  random32* self = check_random32(L);
  if (!self) {
    lua_pushnil(L);
    return 1;
  }
  unsigned int low, up;
  switch (lua_gettop(L)) {
  case 1:
    lua_pushinteger(L, MT19937_random32(self->gen));
    return 1;
  case 2:
    low = 1;
    up = (unsigned int)luaL_checkinteger(L, 2);
    if (up == 0) {
      lua_pushinteger(L, MT19937_random32(self->gen));
      return 1;
    }
    break;
  case 3:
    low = (unsigned int)luaL_checkinteger(L, 2);
    up = (unsigned int)luaL_checkinteger(L, 3);
    break;
  default:
    return luaL_error(L, "wrong number of arguments");
  }
  luaL_argcheck(L, low <= up, 2, "interval is empty");
  lua_pushinteger(L, (lua_Integer)MT19937_random32_range(self->gen, low, up));
  return 1;
}

static int random32_delete(lua_State* L)
{
  random32* self = check_random32(L);
  if (self) {
    MT19937_close32(self->gen);
    self->gen = nullptr;
  }
  return 0;
}

static int random64_create(lua_State* L)
{
  random64* userdata = lexnew_userdata<random64>(L, LIB_RANDOM64NAME);
  userdata->gen = MT19937_state64();
  return 1;
}

static int random64_srand(lua_State* L)
{
  random64* self = check_random64(L);
  if (self) {
    MT19937_srand64(self->gen, luaL_checkinteger(L, 2));
  }
  return 0;
}

static int random64_generate(lua_State* L)
{
  random64* self = check_random64(L);
  if (!self) {
    lua_pushnil(L);
    return 1;
  }
  long long low, up;
  switch (lua_gettop(L)) {
  case 1:
    lua_pushinteger(L, MT19937_random64(self->gen));
    return 1;
  case 2:
    low = 1;
    up = luaL_checkinteger(L, 2);
    if (up == 0) {
      lua_pushinteger(L, MT19937_random64(self->gen));
      return 1;
    }
    break;
  case 3:
    low = luaL_checkinteger(L, 2);
    up = luaL_checkinteger(L, 3);
    break;
  default:
    return luaL_error(L, "wrong number of arguments");
  }
  luaL_argcheck(L, low <= up, 2, "interval is empty");
  lua_pushinteger(L, (lua_Integer)MT19937_random64_range(self->gen, low, up));
  return 1;
}

static int random64_delete(lua_State* L)
{
  random64* self = check_random64(L);
  if (self) {
    MT19937_close64(self->gen);
    self->gen = nullptr;
  }
  return 0;
}

static int random32_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",			  random32_delete		},
    { "close",		  random32_delete		},
    { "srand",			random32_srand		},
    { "generate",		random32_generate	},
    { NULL,				  NULL				      }
  };
  lexnew_metatable(L, LIB_RANDOM32NAME, methods);
  lua_pop(L, 1);
  return 0;
}

static int random64_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",			  random64_delete		},
    { "close",			random64_delete		},
    { "srand",			random64_srand		},
    { "generate",		random64_generate	},
    { NULL,				  NULL				      }
  };
  lexnew_metatable(L, LIB_RANDOM64NAME, methods);
  lua_pop(L, 1);
  return 0;
}

/***********************************************************************************/

extern "C" int luaopen_random(lua_State* L)
{
  luaL_checkversion(L);
  random32_metatable(L);
  random64_metatable(L);
  struct luaL_Reg methods[] = {
    { "random32", random32_create },
    { "random64", random64_create },
    { NULL,		  NULL }
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  return 1;
}

/***********************************************************************************/

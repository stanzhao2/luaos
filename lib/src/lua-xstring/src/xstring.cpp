

/************************************************************************************
**
** Copyright 2021-2023 stanzhao
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

#include "xstring.h"

lua_string::lua_string(lua_State* L)
  : parent(L) {
}

lua_string::lua_string(lua_State* L, const char *s)
  : parent(L), _str(s) {
}

lua_string::lua_string(lua_State* L, const char *s, size_t len)
  : parent(L), _str((int)len, s) {
}

lua_string::lua_string(lua_State* L, const lua_string& b)
  : parent(L), _str(b._str) {
}

const char* lua_string::check_string(int index) const
{
  const char* s = check_userdata<lua_string>(L, index)->_str;
  return s ? s : luaL_checkstring(L, index);
}

int lua_string::split() const
{
  const char *s = _str;
  const char *sep = check_string(2);
  const char *e;
  int i = 1;

  lua_newtable(L);  /* result */
                    /* repeat for each separator */
  while ((e = strchr(s, *sep)) != NULL) {
    lua_pushlstring(L, s, e-s);  /* push substring */
    lua_rawseti(L, -2, i++);
    s = e + 1;  /* skip separator */
  }

  /* push last substring */
  lua_pushstring(L, s);
  lua_rawseti(L, -2, i);
  return 1;  /* return the table */
}

int lua_string::concat() const
{
  lua_string *ns = luaL_newuserdata<lua_string>(L, *this);
  ns->_str += check_string(2);
  return 1;
}

int lua_string::tostring() const
{
  lua_pushlstring(L, _str, _str.length());
  return 1;
}

int lua_string::eq() const
{
  lua_pushboolean(L, (_str == check_string(2)) ? 1 : 0);
  return 1;
}

int lua_string::lt() const
{
  lua_pushboolean(L, (_str < check_string(2)) ? 1 : 0);
  return 1;
}

int lua_string::le() const
{
  lua_pushboolean(L, (_str <= check_string(2)) ? 1 : 0);
  return 1;
}

int lua_string::len() const
{
  lua_pushinteger(L, _str.length());
  return 1;
}

int lua_string::caselesseq() const
{
  CBString b(check_string(2));
  lua_pushboolean(L, _str.caselessEqual(b) ? 1 : 0);
  return 1;
}

int lua_string::find() const
{
  int pos = (int)luaL_optinteger(L, 3, 1);
  if (pos < 1) {
    luaL_error(L, "index starts from 1");
  }
  pos = _str.find(check_string(2), pos - 1);
  pos < 0 ? lua_pushnil(L) : lua_pushinteger(L, (lua_Integer)pos + 1);
  return 1;
}

int lua_string::caselessfind() const
{
  int pos = (int)luaL_optinteger(L, 3, 1);
  if (pos < 1) {
    luaL_error(L, "index starts from 1");
  }
  pos = _str.caselessfind(check_string(2), pos - 1);
  pos < 0 ? lua_pushnil(L) : lua_pushinteger(L, (lua_Integer)pos + 1);
  return 1;
}

int lua_string::reversefind() const
{
  int pos = (int)luaL_optinteger(L, 3, 1);
  if (pos < 1) {
    luaL_error(L, "index starts from 1");
  }
  pos = _str.reversefind(check_string(2), pos - 1);
  pos < 0 ? lua_pushnil(L) : lua_pushinteger(L, (lua_Integer)pos + 1);
  return 1;
}

int lua_string::caselessreversefind() const
{
  int pos = (int)luaL_optinteger(L, 3, 1);
  if (pos < 1) {
    luaL_error(L, "index starts from 1");
  }
  pos = _str.caselessreversefind(check_string(2), pos - 1);
  pos < 0 ? lua_pushnil(L) : lua_pushinteger(L, (lua_Integer)pos + 1);
  return 1;
}

int lua_string::findreplace()
{
  _str.findreplace(
    check_string(2), check_string(3), (int)luaL_optinteger(L, 4, 0)
  );
  lua_pushvalue(L, 1);
  return 1;
}

int lua_string::findreplacecaseless()
{
  _str.findreplacecaseless(
    check_string(2), check_string(3), (int)luaL_optinteger(L, 4, 0)
  );
  lua_pushvalue(L, 1);
  return 1;
}

int lua_string::trim()
{
  _str.trim();
  lua_pushvalue(L, 1);
  return 1;
}

int lua_string::ltrim()
{
  _str.ltrim();
  lua_pushvalue(L, 1);
  return 1;
}

int lua_string::rtrim()
{
  _str.rtrim();
  lua_pushvalue(L, 1);
  return 1;
}

int lua_string::toupper()
{
  _str.toupper();
  lua_pushvalue(L, 1);
  return 1;
}

int lua_string::tolower()
{
  _str.tolower();
  lua_pushvalue(L, 1);
  return 1;
}

/***********************************************************************************/

static int ls_split(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->split() : 0;
}

static int ls_concat(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->concat() : 0;
}

static int ls_tostring(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->tostring() : 0;
}

static int ls_eq(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->eq() : 0;
}

static int ls_lt(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->lt() : 0;
}

static int ls_le(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->le() : 0;
}

static int ls_len(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->len() : 0;
}

static int ls_caselesseq(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->caselesseq() : 0;
}

static int ls_find(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->find() : 0;
}

static int ls_caselessfind(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->caselessfind() : 0;
}

static int ls_reversefind(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->reversefind() : 0;
}

static int ls_caselessreversefind(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->caselessreversefind() : 0;
}

static int ls_findreplace(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->findreplace() : 0;
}

static int ls_findreplacecaseless(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->findreplacecaseless() : 0;
}

static int ls_trim(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->trim() : 0;
}

static int ls_ltrim(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->ltrim() : 0;
}

static int ls_rtrim(lua_State* L)
{
  auto self = check_userdata<lua_string>(L, 1);
  return self ? self->rtrim() : 0;
}

const luaL_Reg* lua_string::methods()
{
  static struct luaL_Reg __methods[] = {
    { "__concat",             ls_concat               },
    { "__tostring",           ls_tostring             },
    { "__eq",                 ls_eq                   },
    { "__lt",                 ls_lt                   },
    { "__le",                 ls_le                   },
    { "__len",                ls_len                  },
    { "split",                ls_split                },
    { "caselesseq",           ls_caselesseq           },
    { "find",                 ls_find                 },
    { "caselessfind",         ls_caselessfind         },
    { "reversefind",          ls_reversefind          },
    { "caselessreversefind",  ls_caselessreversefind  },
    { "findreplace",          ls_findreplace          },
    { "findreplacecaseless",  ls_findreplacecaseless  },
    { "trim",                 ls_trim                 },
    { "ltrim",                ls_ltrim                },
    { "rtrim",                ls_rtrim                },
    { NULL,                   NULL                    },
  };
  return __methods;
}

/***********************************************************************************/

static int ls_new(lua_State* L)
{
  luaL_checkversion(L);
  lua_string* result = nullptr;
  int type = lua_gettop(L) ? lua_type(L, 1) : LUA_TNIL;
  switch (type)
  {
  case LUA_TNIL:
    result = luaL_newuserdata<lua_string>(L);
    break;
  case LUA_TUSERDATA:
    if (result = check_userdata<lua_string>(L, 1)) {
      result = luaL_newuserdata<lua_string>(L, *result);
      break;
    }
  default:
    result = luaL_newuserdata<lua_string>(L, lua_tostring(L, 1));
    break;
  }
  return result ? 1 : 0;
}

extern "C" int luaopen_xstring(lua_State * L)
{
  return lua_pushcfunction(L, ls_new), 1;
}

/***********************************************************************************/

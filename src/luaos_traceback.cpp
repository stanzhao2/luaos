
/************************************************************************************
**
** Copyright 2021-2023 LuaOS
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

#include "luaos.h"

/*
** {======================================================
** Traceback
** =======================================================
*/
#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */

static int findfield(lua_State *L, int objidx, int level)
{
  if (level == 0 || !lua_istable(L, -1))
    return 0;  /* not found */
  lua_pushnil(L);  /* start 'next' loop */
  while (lua_next(L, -2)) {  /* for each pair in table */
    if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
      if (lua_rawequal(L, objidx, -1)) {  /* found object? */
        lua_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
                                                   /* stack: lib_name, lib_table, field_name (top) */
        lua_pushliteral(L, ".");  /* place '.' between the two names */
        lua_replace(L, -3);  /* (in the slot occupied by table) */
        lua_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    lua_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}

/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (lua_State *L, lua_Debug *ar)
{
  int top = lua_gettop(L);
  lua_getinfo(L, "f", ar);  /* push function */
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  if (findfield(L, top + 1, 2))
  {
    const char *name = lua_tostring(L, -1);
    if (strncmp(name, LUA_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      lua_pushstring(L, name + 3);  /* push name without prefix */
      lua_remove(L, -2);  /* remove original name */
    }
    lua_copy(L, -1, top + 1);  /* copy name to proper place */
    lua_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    lua_settop(L, top);  /* remove function and global table */
    return 0;
  }
}

static void pushfuncname(lua_State *L, lua_Debug *ar)
{
  if (ar->source[0] == '@' || ar->source[0] == '=') {
    ar->source += 1;
  }
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    lua_pushfstring(L, "function '%s'", lua_tostring(L, -1));
    lua_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    lua_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
    lua_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
    lua_pushfstring(L, "function <%s:%d>", ar->source, ar->linedefined);
  else  /* nothing left... */
    lua_pushliteral(L, "?");
}

static int lastlevel (lua_State *L)
{
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}

int luaos_traceback(lua_State *L)
{
  int level = 1;
  luaL_Buffer b;
  lua_Debug ar;
  int last = lastlevel(L);
  const char* msg = luaL_checkstring(L, -1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  luaL_buffinit(L, &b);
  if (msg) {
    luaL_addstring(&b, msg);
    luaL_addchar(&b, '\n');
  }
  luaL_addstring(&b, "stack traceback:");
  while (lua_getstack(L, level++, &ar))
  {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      lua_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      luaL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      lua_getinfo(L, "Slnt", &ar);
      if (ar.source[0] == '@' || ar.source[0] == '=') {
        ar.source += 1;
      }
      if (ar.currentline <= 0)
        lua_pushfstring(L, "\n\t%s: in ", ar.source);
      else
        lua_pushfstring(L, "\n\t%s:%d: in ", ar.source, ar.currentline);
      luaL_addvalue(&b);
      pushfuncname(L, &ar);
      luaL_addvalue(&b);
      if (ar.istailcall)
        luaL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  luaL_pushresult(&b);
  return 1;
}

/*******************************************************************************/

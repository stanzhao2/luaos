

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

#pragma once

#include <lua.hpp>
#include <new>  /* include operator new */

/***********************************************************************************/

class lua_userdata {
public:
  inline lua_userdata(lua_State* L) {
    this->L = L;
  }
  virtual ~lua_userdata() {
    close();
  }
  virtual void close() {
    L = nullptr;
  }
public:
  inline static const char* name() {
    return __FUNCTION__;
  }
  inline static const luaL_Reg* methods() {
    return nullptr;
  }
  inline  bool closed() const {
    return (L == nullptr);
  }
protected:
  lua_State* L; /* lua state handle */
};

/***********************************************************************************/

template <typename _Ty>
static _Ty* check_userdata(lua_State* L, int index = 1)
{
  auto ud = (_Ty*)luaL_checkudata(L, index, _Ty::name());
  luaL_argcheck(L, ud, index, "self value expected");
  return (!ud || ud->closed()) ? nullptr : ud;
}

template <typename _Ty>
static int userdata_close(lua_State* L)
{
  auto ud = check_userdata<_Ty>(L);
  if (ud) ud->close();
  return 0;
}

template <typename _Ty>
static int userdata_gc(lua_State* L)
{
  auto ud = check_userdata<_Ty>(L);
  if (ud) ud->~_Ty();
  return 0;
}

template <typename _Ty, typename... Args>
static _Ty* new_userdata(lua_State* L, Args... args)
{
  _Ty* ud = (_Ty*)lua_newuserdata(L, sizeof(_Ty));
  if (ud != nullptr)
  {
    luaL_getmetatable(L, _Ty::name());
    lua_setmetatable(L, -2);
    ud = new (ud) _Ty(L, args...);
  }
  return ud;
}

template <typename _Ty>
static int new_metatable(lua_State* L, const luaL_Reg* methods)
{
  struct luaL_Reg defaults[] = {
    { "__gc",     userdata_gc<_Ty>     },
    { "close",    userdata_close<_Ty>  },
    { NULL,       NULL                 },
  };
  /* define methods */
  if (!luaL_newmetatable(L, _Ty::name())) {
    return 0;
  }
  luaL_setfuncs(L, defaults, 0);
  if (methods) {
    luaL_setfuncs(L, methods, 0);
  }
  /* define metamethods */
  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);
  lua_settable(L, -3);

  lua_pushliteral(L, "__metatable");
  lua_pushliteral(L, "you're not allowed to get this metatable");
  lua_settable(L, -3);
  return 1;
}

/***********************************************************************************/

template <typename _Ty, typename... Args>
static _Ty* luaL_newuserdata(lua_State* L, Args... args)
{
  if (new_metatable<_Ty>(L, _Ty::methods()))
    lua_pop(L, 1);
  return new_userdata<_Ty>(L, args...);
}

/***********************************************************************************/

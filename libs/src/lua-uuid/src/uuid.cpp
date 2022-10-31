

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

#include <lua.hpp>

#if defined(_WIN32)
# include"objbase.h"
#else
# include <uuid/uuid.h>
#endif

/***********************************************************************************/

static int uuid_create(lua_State* L)
{
#if defined(_WIN32)
  GUID guid;
  auto ret = CoCreateGuid(&guid);

  const int len = 64;
  char dst[len];
  memset(dst, 0, len);
  snprintf(dst, len,
    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    guid.Data1, guid.Data2, guid.Data3,
    guid.Data4[0], guid.Data4[1], guid.Data4[2],
    guid.Data4[3], guid.Data4[4], guid.Data4[5],
    guid.Data4[6], guid.Data4[7]);
  lua_pushstring(L, dst);
#else
  uuid_t uu;
  uuid_generate(uu);
  char uuid_str[64];
  uuid_unparse_lower(uu, uuid_str);
  lua_pushstring(L, uuid_str);
#endif
  return 1;
}

/***********************************************************************************/

LUALIB_API int luaopen_uuid(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "generate",   uuid_create },
    { NULL,		      NULL }
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  return 1;
}

/***********************************************************************************/


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

#include <clipp/include/clipp.h>
#include "luaos.h"
#include "luaos_logo.h"

#ifndef _MSC_VER
#include <gperftools/malloc_extension.h>
#include <gperftools/tcmalloc.h>
#else
#include "luaos_compile.h"
#include "dumper/ifdumper.h"
static CMiniDumper _G_dumper(true);
#endif

/***********************************************************************************/

static const char* normal(const char* name)
{
  static std::string data;
  size_t namelen = strlen(name);
  if (namelen > 4)
  {
    char extname[32];
    strcpy(extname, name + namelen - 4);
    char* p = extname;
    for(; *p; p++)
    {
      char c = *p;
      if (c >= 'A' && c <= 'Z') {
        *p += ('a' - 'A');
      }
    }
    if (strcmp(extname, ".lua") == 0) {
      data.append(name, namelen - 4);
      return data.c_str();
    }
  }
  return name;
}

static int pmain(lua_State* L)
{
  luaL_checkversion(L);
  int  argc = (int)lua_tointeger(L, 1);
  auto argv = (const char **)lua_touserdata(L, 2);
  const char* name = argc > 1 ? argv[1] : luaos_fmain;
  name = normal(name);

  luaos_trace("LuaOS has been started\n");
  int result = luaos_pexec(L, name, 0);   /* load and run module in protect mode */
  if (result != LUA_OK) {
    lua_error(L);                         /* run error */
  }
  lua_pushinteger(L, result);             /* push result to stack */
  return 1;
}

int main(int argc, char* argv[])
{
  display_logo();
#ifndef _MSC_VER
  MallocExtension::instance()->SetMemoryReleaseRate(0);
#else
  using namespace clipp;
  bool compile = false;
  std::string filename;
  auto cli = (
    opt_value("filename", filename),
    option("-c", "--compile").set(compile) & value("filename", filename)
  );
  if (!parse(argc, argv, cli)) {
    luaos_error("Invalid command line argument\n\n");
    return 1;
  }
#endif
  lua_Integer error = -1;
  lua_State* L = luaos_local.lua_state();
  if (compile) {
    luaos_trace("Prepare to compile and copy files\n");
    int count = luaos_compile(L, filename.c_str());
    luaos_trace("Successfully compiled %d lua files\n\n", count);
    return 0;
  }

  lua_pushcfunction(L, &pmain);           /* to call 'pmain' in protected mode */
  lua_pushinteger(L, argc);               /* 1st argument */
  lua_pushlightuserdata(L, argv);         /* 2nd argument */
  if (luaos_pcall(L, 2, 1) == LUA_OK) {   /* do the call */
    error = lua_tointeger(L, -1);         /* get result */
  }
  else {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  luaos_close(L);
  luaos_trace("LuaOS has exited, see you...\n\n");
  return (int)error;
}

/***********************************************************************************/

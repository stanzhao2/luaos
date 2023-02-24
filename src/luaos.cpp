
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
#include <md5.h>
#include "luaos.h"
#include "luaos_mc.h"
#include "luaos_logo.h"
#include "luaos_compile.h"

#ifndef _MSC_VER
#include <gperftools/malloc_extension.h>
#include <gperftools/tcmalloc.h>
#else
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
  const char* name = luaL_checkstring(L, 1);
  name = normal(name);

  int result = luaos_pexec(L, name, 0);   /* load and run module in protect mode */
  if (result != LUA_OK) {
    lua_error(L);                         /* run error */
  }
  lua_pushinteger(L, result);             /* push result to stack */
  return 1;
}

static void replace(std::string& filename)
{
  for (size_t i = 0; i < filename.size(); i++) {
    if (filename[i] == '\\') {
      filename[i] = '/';
    }
  }
}

int main(int argc, char* argv[])
{
  display_logo();

#ifdef _MSC_VER
  setlocale(LC_ALL, "");
  SetConsoleOutputCP(936);
#else
  MallocExtension::instance()->SetMemoryReleaseRate(0);
#endif

  using namespace clipp;
  bool compile = false, bimport = false;
  bool help = false, usekey = false, bexport = false;
  std::string fmain(luaos_fmain), filename, strkey;
  std::vector<std::string> extnames;

  auto cli = (
    opt_value("filename", fmain),
    option("-h", "/h", "--help").set(help),
    option("-i", "/i", "--input").set(bimport) & value("filename", filename),
    option("-c", "/c", "--compile").set(compile) & value("filename", filename) & repeatable(opt_value("ext", extnames)),
    option("-e", "/e", "--export").set(bexport) & value("filename", filename),
    option("-k", "/k", "--key").set(usekey) & value("key", strkey)
  );

  extnames.push_back("lua");
  if (!parse(argc, argv, cli) || (compile && bimport) || fmain[0] == '-') {
    luaos_error("Invalid parameter, use -h to view help\n\n");
    return 0;
  }

  luaos_trace("LuaOS has been started\n");
  lua_Integer error = -1;
  lua_State* L = luaos_local.lua_state();

  if (help) {
    std::string fname(argv[0]);
    const char* pos = strrchr(argv[0], LUA_DIRSEP[0]);
    if (pos) {
      fname = pos + 1;
    }
    printf(
      "  > usage: %s [module] [options]\n"
      "  > Available options are:\n"
      "  >   -h show this usage\n"
      "  >   -c filename [[*] | [extensions]] output to file 'filename'\n"
      "  >   -i filename import from file 'filename'\n"
      "  >   -e filename export from file 'filename'\n"
      "  >   -k key for import/export\n",
      fname.c_str()
    );
    return 0;
  }
  if (compile) {
    std::set<std::string> exts;
    for (size_t i = 0; i < extnames.size(); i++) {
      exts.insert(extnames[i]);
    }
    luaos_trace("Prepare to build project files\n");
    replace(filename);
    int count = luaos_compile(L, filename.c_str(), exts, strkey.c_str());
    luaos_trace("Successfully build %d files\n\n", count);
    return 0;
  }
  if (bexport) {
    replace(filename);
    int count = luaos_export(L, filename.c_str(), strkey.c_str(), true);
    if (count < 0) {
      luaos_trace("Please check whether the key entered is correct\n\n");
      return 1;
    }
    luaos_trace("Successfully export %d files\n\n", count);
    return 0;
  }
  if (bimport) {
    replace(filename);
    int count = luaos_export(L, filename.c_str(), strkey.c_str(), false);
    if (count < 0) {
      luaos_trace("Please check whether the key entered is correct\n\n");
      return 1;
    }
    luaos_trace("Successfully import %d files\n", count);
  }

  lua_pushcfunction(L, &pmain);           /* to call 'pmain' in protected mode */
  lua_pushstring(L, fmain.c_str());       /* 1st argument */
  if (luaos_pcall(L, 1, 1) == LUA_OK) {   /* do the call */
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

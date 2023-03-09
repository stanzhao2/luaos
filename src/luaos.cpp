
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
#include "luaos_master.h"
#include "luaos_mc.h"
#include "luaos_logo.h"
#include "luaos_compile.h"

static identifier _G_;

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

static int start_master(lua_State* L)
{
  const char* host = luaL_checkstring(L, 1);
  unsigned short port = (unsigned short)luaL_checkinteger(L, 2);
  int threads = (int)luaL_optinteger(L, 3, 1);
  lua_pushboolean(L, luaos_start_master(host, port, threads) ? 1 : 0);
  return 1;
}

static int stop_master(lua_State* L)
{
  luaos_stop_master();
  return 0;
}

lua_State* init_main_state()
{
  lua_State* L = luaos_local.lua_state();
#if 0
  lua_getglobal(L, "os");   /* push os from stack */
  lua_newtable(L);
  lua_pushcfunction(L, start_master);
  lua_setfield(L, -2, "start");
  lua_pushcfunction(L, stop_master);
  lua_setfield(L, -2, "stop");
  lua_setfield(L, -2, "master");
  lua_pop(L, 1);  /* pop os from stack */
#endif
  return L;
}

int main(int argc, char* argv[])
{
  display_logo();
#ifndef _MSC_VER
  MallocExtension::instance()->SetMemoryReleaseRate(0);
#else
  SetConsoleOutputCP(65001);  /* set charset to utf8 */
#endif

  using namespace clipp;
  bool cmd_help     = false;
  bool cmd_compile  = false;
  bool cmd_filename = false;
  bool cmd_key      = false;
  bool cmd_unpack   = false;

  std::string filename, filekey;
  std::string main_name(luaos_fmain);
  std::vector<std::string> filestype;

  auto cmd = (
    option("-h", "/h", "--help").set(cmd_help) | (
      (opt_value("module-name", main_name)),
      (option("-b", "/b", "--build").set(cmd_compile) & repeatable(opt_value("files-type", filestype))) |
      (option("-u", "/u", "--unpack").set(cmd_unpack)),
      (option("-f", "/f", "--filename").set(cmd_filename) & value("filename", filename)),
      (option("-p", "/p", "--password").set(cmd_key) & value("key", filekey))
    )
  );

  filestype.push_back("lua");
  if (!parse(argc, argv, cmd) || main_name[0] == '-') {
    luaos_error("Invalid command, use -h to view help\n\n");
    return 0;
  }

  if (cmd_help) {
    std::string exefname(argv[0]);
    const char* pos = strrchr(argv[0], LUA_DIRSEP[0]);
    if (pos) {
      exefname = pos + 1;
    }
    printf(
      "> usage: %s [module] [options]\n"
      "> Available options are:\n"
      ">   -h .... show this usage\n"
      ">   -b .... build [[all] or [list of file extensions]]\n"
      ">   -u .... unpack\n"
      ">   -f .... input/output file\n"
      ">   -p .... password for input/output file\n\n",
      exefname.c_str()
    );
    return 0;
  }

  if (filename.empty()) {
    time_t now = time(0);
    auto ptm = localtime(&now);
    char name[256];
    snprintf(name, sizeof(name), "%02d%02d%02d%02d%02d%02d.img", 1900 + ptm->tm_year, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    filename.assign(name);
  }
  if (filekey.empty()) {
    filekey = "11852234-3e57-484d-b128-cd99a6b76204";
  }

  unsigned char hash[16];
  int size = md5::hash(filekey.c_str(), filekey.size(), hash);
  filekey.clear();
  for (int i = 0; i < size; i++) {
    char hex[3];
    sprintf(hex, "%02x", hash[i]);
    filekey.append(hex);
  }
  const char* password = filekey.c_str();

  lua_State* L = init_main_state();
  if (cmd_compile) {
    std::set<std::string> exts;
    for (size_t i = 0; i < filestype.size(); i++) {
      exts.insert(filestype[i]);
    }
    replace(filename);
    luaos_compile(L, filename.c_str(), exts, password);
    return printf("\n");
  }
  if (cmd_unpack) {
    replace(filename);
    luaos_export(L, filename.c_str(), password, true);
    return printf("\n");
  }
  if (cmd_filename) {
    replace(filename);
    if (luaos_export(L, filename.c_str(), password, false) < 0) {
      return printf("\n");
    }
  }

  lua_Integer error = -1;
  luaos_trace("Starting LuaOS...\n");
  lua_pushcfunction(L, &pmain);           /* to call 'pmain' in protected mode */
  lua_pushstring(L, main_name.c_str());   /* 1st argument */
  if (luaos_pcall(L, 1, 1) == LUA_OK) {   /* do the call */
    error = lua_tointeger(L, -1);         /* get result */
  }
  else {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  luaos_trace("LuaOS has exited, see you...\n\n");
  return (int)error;
}

/***********************************************************************************/

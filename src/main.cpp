
/********************************************************************************
** 
** Copyright 2021-2022 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
********************************************************************************/

#include <iostream>
#include <signal.h>
#include <thread>
#include <memory>
#include <conv.h>
#include "clipp/include/clipp.h"

#include "luaos.h"
#include "luaos_loader.h"
#include "luaos_thread_local.h"
#include "luaos_rpc.h"
#include "luaos_job.h"
#include "luaos_socket.h"
#include "luaos_twheel.h"
#include "luaos_storage.h"
#include "luaos_subscriber.h"

#ifndef OS_WINDOWS
#include <gperftools/malloc_extension.h>
#include <gperftools/tcmalloc.h>
#else
#include "dumper/ifdumper.h"
static CMiniDumper _G_dumper(true);
#endif

/*******************************************************************************/

static const identifier hold_id;
static reactor::ref main_ios = 0;
static const char* rom_fname = 0;
static const char* mod_fname = LUAOS_MAIN;

/*******************************************************************************/

static int lua_loader(lua_State* L)
{
  if (!rom_fname) { //no rom file
    return 0;
  }
  std::string luafile;
  const char* filename = luaL_checkstring(L, -1);

  bool ok = lua_decode(filename, rom_fname, luafile);
  if (!ok) {
    return 0;
  }

  lua_getglobal(L, "_DEBUG");
  int debug_type = lua_type(L, -1);
  lua_pop(L, 1);

  if (debug_type != LUA_TNIL) {
    lua_pushnil(L);
    lua_setglobal(L, "_DEBUG");
  }

  lua_pushlstring(L, luafile.c_str(), luafile.size());
  return 1;
}

static void init_folder(lua_State* L, const char* name, const char* path)
{
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_pushstring(L, path);
  lua_setfield(L, -2, name);
  lua_pop(L, 1);
}

static int throw_error(lua_State* L)
{
  std::string name = luaL_checkstring(L, -2);
  if (lua_isfunction(L, -1))
  {
    if (name == "main") {
      lua_rawset(L, 1);
      return 0;
    }
  }
  return luaL_error(L, "Global variables cannot be used: %s", name.c_str());
}

static int lua_string_split(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  const char *sep = luaL_checkstring(L, 2);
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

static void lua_protect(lua_State* L, lua_CFunction fn)
{
  lua_getglobal(L, "_G");
  lua_getfield(L, -1, "__newindex");
  if (lua_isfunction(L, -1)) { //已添加过了
    lua_pop(L, 2);
    return;
  }

  lua_pop(L, 1);
  luaL_newmetatable(L, "placeholders");
  lua_pushliteral(L, "__newindex");
  lua_pushcfunction(L, fn);

  lua_rawset(L, -3);
  lua_setmetatable(L, -2);
  lua_pop(L, 1);
}

static int lua_os_system_clock(lua_State* L)
{
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  ).count();
  lua_pushinteger(L, ms);
  return 1;
}

static int lua_os_steady_clock(lua_State* L)
{
  auto ms = os::milliseconds();
  lua_pushinteger(L, (lua_Integer)ms);
  return 1;
}

static void init_lua_state(lua_State* L)
{
  lua_getglobal(L, "error");
  lua_setglobal(L, "throw");

  lua_pushcfunction(L, lua_os_print);
  lua_setglobal(L, "print");

  lua_pushcfunction(L, lua_os_error);
  lua_setglobal(L, "error");

  lua_pushcfunction(L, lua_os_trace);
  lua_setglobal(L, "trace");

  lua_getglobal(L, "os");
  lua_pushcfunction(L, lua_os_system_clock);
  lua_setfield(L, -2, "system_clock");

  lua_pushcfunction(L, lua_os_steady_clock);
  lua_setfield(L, -2, "steady_clock");

  lua_pushcfunction(L, lua_os_files);
  lua_setfield(L, -2, "files");

  lua_pushcfunction(L, lua_os_id);
  lua_setfield(L, -2, "id");

  lua_pushcfunction(L, lua_os_typename);
  lua_setfield(L, -2, "typename");

  lua_pushcfunction(L, lua_os_deadline);
  lua_setfield(L, -2, "deadline");
  lua_pop(L, 1); //pop os from stack

  lua_getglobal(L, "string");
  lua_pushcfunction(L, lua_string_split);
  lua_setfield(L, -2, "split");
  lua_pop(L, 1); //pop string from stack

  lua_getglobal(L, "debug");
  lua_pushcfunction(L, lua_traceback);
  lua_setfield(L, -2, "traceback");
  lua_pop(L, 1); //pop debug from stack

  lua_getglobal(L, "utf8");
  if (lua_type(L, -1) == LUA_TNIL)
  {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "utf8");
  }
  lua_pushcfunction(L, lua_is_utf8);
  lua_setfield(L, -2, "check");
  lua_pop(L, 1); //pop utf8 from stack
}

static void init_preload(lua_State* L)
{
  static luaL_Reg modules[] = {
    { NULL,     NULL          }
  };

  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield (L, -1, "preload");
  for (const luaL_Reg* lib = modules; lib->func; lib++)
  {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 2);
}

static void init_metatable(lua_State* L)
{
  lua_twheel::init_metatable(L);
  luaos_subscriber::init_metatable(L);
  storage::init_metatable(L);
  rpc::init_metatable(L);

  lua_job::init_metatable(L);
  lua_socket::init_metatable(L);
  lua_socket::init_ssl_metatable(L);
}

static void signal_handler(int code)
{
  if (code == SIGINT || code == SIGTERM) {
    if (main_ios) {
      main_ios->stop();
    }
  }

  signal(code, signal_handler);
}

/*******************************************************************************/

lua_State* new_lua_state()
{
  lua_State* L = this_thread().lua_state();
  if (L)
  {
    luaL_openlibs(L);
    lua_pushboolean(L, 1);
    lua_setglobal(L, "_DEBUG");

#if defined LUA_VERSION_NUM && LUA_VERSION_NUM >= 504 
    lua_gc(L, LUA_GCGEN, 0, 0);
#endif

    int topindex = lua_gettop(L);
    lua_new_loader(L, lua_loader);

    init_folder(L, "path",  LUAOS_FILE_PATH);
    init_folder(L, "cpath", LUAOS_LIBS_PATH);

    init_lua_state(L);
    init_metatable(L);

    init_preload(L);

    lua_protect(L, throw_error);
    lua_settop(L, topindex);
  }
  return L;
}

static int __deadline_time = 30;
static std::map<lua_State*, int> __lua_runtime;

LUALIB_API int lua_os_deadline(lua_State* L)
{
  size_t deadline = luaL_checkinteger(L, 1);
  if (deadline < 10) {
    deadline = 10;
  }
  if (deadline > 86400) {
    deadline = 86400;
  }
  __deadline_time = (int)deadline;
  return 0;
}

void post_alive_exit(lua_State* L, twheel_t* tw)
{
  reactor_type ios = check_ios();
  ios->post(
    [L, tw]() {
      __lua_runtime.erase(L);
      lua_close(L);
      timewheel_release(tw);
    }
  );
}

void post_alive(lua_State* L)
{
  reactor_type ios = check_ios();
  ios->post(
    [L]() {
      auto iter = __lua_runtime.find(L);
      if (iter == __lua_runtime.end()) {
        __lua_runtime[L] = 0;
      }
      else {
        iter->second = 0;
      }
    }
  );
}

const char* module_fname()
{
  return mod_fname;
}

static void interrupt(lua_State *L, lua_Debug *ar)
{
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "run timeout, interrupted!");
}

static void check_thread()
{
  reactor_type ios = check_ios();
  while (!ios->stopped())
  {
    ios->run_for(std::chrono::milliseconds(1000));
    auto iter = __lua_runtime.begin();
    for (; iter != __lua_runtime.end(); iter++)
    {
      lua_State* L = iter->first;
      if (lua_gethookmask(L)) {
        continue;
      }
      iter->second++;
      if (iter->second < __deadline_time) {
        continue;
      }
      iter->second = 0;
      lua_sethook(L, interrupt, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
    }
  }
}

static int pmain(lua_State* L)
{
  main_ios = this_thread().lua_reactor();
  _printf(color_type::yellow, true, "loading %s module...\n", mod_fname);

  int result = lua_dofile(L, mod_fname);
  if (result != LUA_OK) {
    luaos_pop_error(L, mod_fname);
  }
  else
  {
    lua_pushcfunction(L, lua_pcall_error);
    int error_fn_index = lua_gettop(L);
    lua_getglobal(L, LUAOS_MAIN);

    if (lua_isfunction(L, -1))
    {
      int argc = (int)lua_tointeger(L, 1);
      char **argv = (char **)lua_touserdata(L, 2);

      for (size_t i = 1; i < argc; i++) {
        lua_pushstring(L, argv[i]);
      }
      _printf(color_type::yellow, true, "running main function in protected mode\n");
      result = lua_pcall(L, argc - 1, 0, error_fn_index);
    }
  }

  lua_pop(L, lua_gettop(L));
  _printf(color_type::yellow, true, "LuaOS stopped, Goodbye!!\n\n");
  lua_pushboolean(L, result == LUA_OK ? 1 : 0);
  return 1;
}

static void wait_exit(std::thread& thd)
{
  if (thd.joinable()) {
    check_ios()->poll();
    check_ios()->stop(), thd.join(); //wait for thread exit
  }
}

int main(int argc, char* argv[])
{
#ifndef OS_WINDOWS
  MallocExtension::instance()->SetMemoryReleaseRate(0);
#else
  HANDLE original = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO defaultScreenInfo;
  GetConsoleScreenBufferInfo(original, &defaultScreenInfo);
  WORD default_color = defaultScreenInfo.wAttributes;
#endif
  std::thread check_thd(check_thread);

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);

  std::string rundir = dir::current();
  int result = chdir(rundir.c_str());

  using namespace clipp;
  std::string infile, modname;
  bool opt_build = false;
  auto cli = (
    option("-f") & value("rom filename", infile),
    option("-c", "--compile").set(opt_build),
    option("-m", "--module") & value("module name", modname)
  );

  if (argc > 1) {
    if (!parse(argc, argv, cli)) {
      wait_exit(check_thd);
#ifdef OS_WINDOWS
      SetConsoleTextAttribute(original, default_color);
#endif
      return EXIT_FAILURE;
    }
    if (!modname.empty()) {
      mod_fname = modname.c_str();
    }
  }

  print_copyright();

#ifdef OS_WINDOWS
  char env[2048] = { ".\\lib;" };
  GetEnvironmentVariableA("PATH", env + 6, sizeof(env) - 6);
  SetEnvironmentVariableA("PATH", env);
#endif

  //check bom header of lua files
  _printf(color_type::yellow, true, "checking file encoding...\n");
  if (!check_bom_header()) {
    printf("\n");
    wait_exit(check_thd);
#ifdef OS_WINDOWS
    SetConsoleTextAttribute(original, default_color);
#endif
    return EXIT_FAILURE;
  }

  if (opt_build)
  {
    _printf(color_type::yellow, true, "compiling lua files...\n");
    if (infile.empty()) {
      infile = "main.bin";
    }
    lua_compile(infile.c_str());
    _printf(color_type::yellow, true, "compilation completed\n\n");
    wait_exit(check_thd);
#ifdef OS_WINDOWS
    SetConsoleTextAttribute(original, default_color);
#endif
    return EXIT_SUCCESS;
  }

  if (!infile.empty()) {
    if (access(infile.c_str(), 0) < 0)
    {
      _printf(color_type::red, true, "%s not found\n\n", infile.c_str());
      wait_exit(check_thd);
#ifdef OS_WINDOWS
      SetConsoleTextAttribute(original, default_color);
#endif
      return EXIT_FAILURE;
    }
    rom_fname = infile.c_str();
  }

  lua_State* L = new_lua_state();  /* create state */
  if (L == NULL) {
    _printf(
      color_type::red, true, "lua_State initialization failed\n\n"
    );
    wait_exit(check_thd);
#ifdef OS_WINDOWS
    SetConsoleTextAttribute(original, default_color);
#endif
    return EXIT_FAILURE;
  }

  lua_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  lua_pushinteger(L, argc);  /* 1st argument */
  lua_pushlightuserdata(L, argv); /* 2nd argument */
  int status = lua_pcall(L, 2, 1, 0);  /* do the call */
  result = lua_toboolean(L, -1);  /* get result */
  lua_close(L);

  wait_exit(check_thd);
#ifdef OS_WINDOWS
  SetConsoleTextAttribute(original, default_color);
#endif
  return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*******************************************************************************/

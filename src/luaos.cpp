
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
#include <iostream>
#include <conv.h>
#include <md5.h>
#include "luaos.h"
#include "luaos_master.h"
#include "luaos_mc.h"
#include "luaos_logo.h"
#include "luaos_compile.h"

/***********************************************************************************/

#ifndef _MSC_VER
#include <gperftools/malloc_extension.h>
#include <gperftools/tcmalloc.h>
#else
#include "dumper/ifdumper.h"
static CMiniDumper _G_dumper(true);
#endif

/***********************************************************************************/

#ifdef _DEBUG
static bool _G_debug = true;
#else
static bool _G_debug = false;
#endif

static identifier  _G_;
static std::string _G_name;
static eth::ip::udp::endpoint _logspeer;
static auto logios = eth::reactor::create();
static auto logsock = eth::socket::create(logios, eth::socket::family::sock_dgram);
static auto logluaL = luaL_newstate();

static const char* tynames[] = {
  "print", "trace", "error"
};

static void sendto_server(const std::string& data, color_type color)
{
  error_code ec;
  if (!logsock->is_open()) {
    ec = logsock->bind(0, "0.0.0.0");
  }
  if (ec) {
    return;
  }
  lua_State* L = logluaL;
  lua_newtable(L);

  lua_pushstring(L, _G_name.c_str());
  lua_setfield(L, -2, "module");

  lua_pushstring(L, tynames[(int)color]);
  lua_setfield(L, -2, "type");

  lua_pushlstring(L, data.c_str(), data.size());
  lua_setfield(L, -2, "message");

  std::string packet;
  lua_packany(L, -1, packet);
  lua_pop(L, 1);
  logsock->send_to(packet.c_str(), packet.size(), _logspeer, ec);
}

void luaos_savelog(const std::string& data, color_type color)
{
  if (logsock && _logspeer.port()) {
    sendto_server(data, color);
  }
}

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

static bool is_integer(const char* p)
{
  while (*p) {
    if (*p < '0' || *p > '9') return false;
    p++;
  }
  return true;
}

static bool is_number(const char* p)
{
  int dotcnt = 0;
  while (*p) {
    if (*p < '0' || *p > '9') {
      if (*p != '.' || dotcnt++) {
        return false;
      }
    }
    p++;
  }
  return true;
}

static bool is_boolean(const char* p)
{
  return (strcmp(p, "true") == 0 || strcmp(p, "false") == 0);
}

static void push_param(lua_State* L, const std::string& v)
{
  const char* p = v.c_str();
  if (is_integer(p)) {
    lua_pushinteger(L, std::stoll(v));
    return;
  }
  if (is_number(p)) {
    lua_pushnumber(L, std::stod(v));
    return;
  }
  if (is_boolean(p)) {
    lua_pushboolean(L, v == "true" ? 1 : 0);
    return;
  }
  lua_pushlstring(L, v.c_str(), v.size());
}

/***********************************************************************************/

bool luaos_is_debug()
{
  return _G_debug;
}

static int pmain(lua_State* L)
{
  luaL_checkversion(L);
  const char* name = luaL_checkstring(L, 1);
  auto argvs = (const std::vector<std::string>*)lua_touserdata(L, 2);
  name = normal(name);

  if (argvs) {
    for (size_t i = 0; i < argvs->size(); i++) {
      push_param(L, argvs->at(i));
    }
  }
  int argc = argvs ? (int)argvs->size() : 0;
  int result = luaos_pexec(L, name, argc);  /* load and run module in protect mode */
  if (result != LUA_OK) {
    lua_error(L); /* run error */
  }
  lua_pushinteger(L, result); /* push result to stack */
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
#ifndef _MSC_VER
  MallocExtension::instance()->SetMemoryReleaseRate(0);
#else
  SetConsoleOutputCP(65001);  /* set charset to utf8 */
#endif
  display_logo();

  using namespace clipp;
  bool cmd_help      = false;
  bool cmd_compile   = false;
  bool cmd_filename  = false;
  bool cmd_key       = false;
  bool cmd_unpack    = false;
  bool cmd_params    = false;
  bool cmd_loghosten = false;

  std::string main_name(luaos_fmain);
  std::string filename, filekey, loghost, logport;
  std::vector<std::string> filestype, luaparams;

  auto cmd = (
    option("-h", "--help").set(cmd_help).doc("display this help") | (
      (opt_value("module", main_name)),
      (option("-f", "--file"  ).set(cmd_filename ).doc("name of image file") & value("filename", filename)),
      (option("-d", "--debug" ).set(_G_debug     ).doc("run in debug mode")),
      (option("-k", "--key"   ).set(cmd_key      ).doc("password of image file") & value("key", filekey)),
      (option("-a", "--argv"  ).set(cmd_params   ).doc("parameters to be passed to lua") & repeatable(opt_value("parameters", luaparams))),
      (option("-l", "--log"   ).set(cmd_loghosten).doc("host and port of remote log server") & value("host", loghost) & value("port", logport)),
      (option("-p", "--pack"  ).set(cmd_compile  ).doc("package files to image file") & repeatable(opt_value("all", filestype))) |
      (option("-u", "--unpack").set(cmd_unpack   ).doc("unpack image file"))
    )
  );

  filestype.push_back("lua");
  if (!parse(argc, argv, cmd) || main_name[0] == '-') {
    cmd_help = true;
  }
  if (!logport.empty() && !is_integer(logport.c_str())) {
    cmd_help = true;
  }
  _G_name = main_name;  /* save to global */

  if (cmd_help) {
    std::string exefname(argv[0]);
    const char* pos = strrchr(argv[0], LUA_DIRSEP[0]);
    if (pos) {
      exefname = pos + 1;
    }
    auto fmt = doc_formatting{}
      .first_column(5)
      .doc_column(20);
    std::cout << make_man_page(cmd, exefname.c_str(), fmt) << "\n";
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
  if (cmd_loghosten) {
    error_code ec;
    auto port = (unsigned short)std::stoi(logport);
    auto result = eth::udp::resolve(loghost.c_str(), port, ec);
    if (!ec) {
      _logspeer = *result.begin();
    }
    else {
      luaos_error("%s\n", ec.message().c_str());
    }
  }
  lua_Integer error = -1;
  luaos_trace("Starting LuaOS...\n");
  lua_pushcfunction(L, &pmain);           /* to call 'pmain' in protected mode */
  lua_pushstring(L, main_name.c_str());   /* 1st argument */
  lua_pushlightuserdata(L, cmd_params ? &luaparams : nullptr);  /* 2st argument */
  if (luaos_pcall(L, 2, 1) == LUA_OK) {   /* do the call */
    error = lua_tointeger(L, -1);         /* get result */
  }
  else {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  if (logsock && logsock->is_open()) {
    logsock->close();
    logsock.reset();
    lua_close(logluaL);
  }
  luaos_close(L);
  luaos_trace("LuaOS has exited, see you...\n\n");
  return (int)error;
}

/***********************************************************************************/

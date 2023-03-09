
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

static identifier  _G_;
static std::string _loghosten;
static std::string _logserver;
static unsigned short _logsvr_port = 0;

static void sendto_server(const std::string& data, color_type color)
{
  static error_code ec;
  static auto ios = eth::reactor::create();
  static auto udpsock = eth::socket::create(ios, eth::socket::family::sock_dgram);
  static const auto peer = eth::udp::resolve(_logserver.c_str(), _logsvr_port, ec);
  if (ec) {
    return;
  }
  if (!udpsock->is_open()) {
    if (ec = udpsock->bind(0, "0.0.0.0")) {
      return;
    }
  }
  std::string packet;
  packet.append((char*)&color, 1);
  packet.append(data);
  packet.append("\0", 1);
  udpsock->send_to(packet.c_str(), packet.size(), *peer.begin(), ec);
  if (ec) {
    ec.clear();
  }
}

void luaos_savelog(const std::string& data, color_type color)
{
  if (_loghosten.empty()) {
    return; /* unknow log server */
  }
  if (!_logsvr_port) {
    auto pos = _loghosten.find(':');
    if (pos == std::string::npos) {
      _logsvr_port = 1;
      return;
    }
    _logserver   = _loghosten.substr(0, pos);
    _logsvr_port = (unsigned short)std::stoi(_loghosten.substr(pos + 1));
  }
  if (!_logserver.empty()) {
    sendto_server(data, color);
  }
}

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
  display_logo();
#ifndef _MSC_VER
  MallocExtension::instance()->SetMemoryReleaseRate(0);
#else
  SetConsoleOutputCP(65001);  /* set charset to utf8 */
#endif

  using namespace clipp;
  bool cmd_help      = false;
  bool cmd_compile   = false;
  bool cmd_filename  = false;
  bool cmd_key       = false;
  bool cmd_unpack    = false;
  bool cmd_params    = false;
  bool cmd_loghosten = false;

  std::string filename, filekey;
  std::string main_name(luaos_fmain);
  std::vector<std::string> filestype, luaparams;

  auto cmd = (
    option("-h").set(cmd_help).doc("...... display help information") | (
      (opt_value("module-name", main_name)),
      (option("-b").set(cmd_compile).doc("...... build image file") & repeatable(opt_value("extensions", filestype))) |
      (option("-u").set(cmd_unpack).doc("...... unpack image file")),
      (option("-f").set(cmd_filename).doc("...... set image file name") & value("filename", filename)),
      (option("-p").set(cmd_key).doc("...... set image file password") & value("password", filekey))
    ) | (
      (opt_value("module-name", main_name)),
      (option("-f").set(cmd_filename).doc("...... set image file name") & value("filename", filename)),
      (option("-p").set(cmd_key).doc("...... set image file password") & value("password", filekey)),
      (option("-a").set(cmd_params).doc("...... parameters passed to lua") & repeatable(opt_value("argvs", luaparams))),
      (option("-l").set(cmd_loghosten).doc("...... remote logserver host:port") & value("hostname", _loghosten))
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
    auto fmt = doc_formatting{}
      .first_column(5)
      .doc_column(8);
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
  luaos_trace("LuaOS has exited, see you...\n\n");
  return (int)error;
}

/***********************************************************************************/


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

#include <string>
#include <mutex>
#include <map>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>
#include <conv.h>

#include "luaos.h"
#include "luaos_state.h"
#include "rapidjson/rapidjson.h"
#include "luaos_socket.h"
#include "luaos_list.h"
#include "luaos_compile.h"
#include "luaos_conv.h"
#include "luaos_pack.h"
#include "luaos_rpcall.h"
#include "luaos_storage.h"
#include "luaos_subscriber.h"
#include "luaos_traceback.h"

#ifdef _MSC_VER
#include "luaos_console.h"
#endif

/***********************************************************************************/
//common functions
/***********************************************************************************/

static io_handler alive_exit = luaos_ionew();

inline static int is_success(int status) {
  return status == LUA_OK || status == LUA_YIELD;
}

/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall(lua_State *L, int status, lua_KContext extra)
{
  if (!is_success(status)) {  /* error? */
    if (extra == 2) {
      lua_pushboolean(L, 0);  /* first result (false) */
      lua_pushvalue(L, -2);   /* error message */
      return 2;  /* return false, msg */
    }
    lua_error(L);
  }
  return lua_gettop(L) - (int)extra;  /* return all results */
}

/*
** Do a protected call with error handling. After 'lua_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int xpcall(lua_State *L)
{
  int status;
  int n = lua_gettop(L);
  luaL_checktype(L, 2, LUA_TFUNCTION);  /* check error function */
  lua_pushboolean(L, 1);  /* first result */
  lua_pushvalue(L, 1);    /* function */
  lua_rotate(L, 3, 2);    /* move them below function's arguments */
  status = lua_pcallk(L, n - 2, LUA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}

static int pcall(lua_State* L)
{
  luaL_checktype(L, 1, LUA_TFUNCTION);  /* check pcall function */
  lua_pushcfunction(L, luaos_traceback);
  lua_insert(L, 2);
  return xpcall(L);
}

int luaos_pcall(lua_State* L, int n, int r)
{
  int index = lua_gettop(L) - n;
  luaL_checktype(L, index, LUA_TFUNCTION);  /* check pcall function */
  lua_pushcfunction(L, luaos_traceback);
  lua_insert(L, index);
  int status = lua_pcallk(L, n, r, index, 1, finishpcall);
  lua_remove(L, index); /* remove traceback from stack */
  return status;
}

static int luaos_bind(lua_State* L)
{
  static auto callback = [](lua_State* L)
  {
    int upvcnt = (int)luaL_checkinteger(L, lua_upvalueindex(1));
    for (int i = 2; i <= upvcnt; i++) {
      lua_pushvalue(L, lua_upvalueindex(i));
    }
    if (upvcnt > 1) {
      lua_rotate(L, 1, upvcnt - 1);
    }
    int status = luaos_pcall(L, lua_gettop(L) - 1, LUA_MULTRET);
    if (status != LUA_OK) {
      luaL_error(L, lua_tostring(L, -1));
    }
    return lua_gettop(L);
  };
  int upvcnt = lua_gettop(L);
  lua_pushinteger(L, upvcnt + 1);
  lua_rotate(L, 1, 1);
  lua_pushcclosure(L, callback, upvcnt + 1);
  return 1;
}

static void check_function(lua_State* L)
{
  auto i = (lua_type(L, 1) == LUA_TTABLE);
  luaL_argcheck(L, i, 1, "table expected");
  lua_pushnil(L);
  if (lua_next(L, 1) == 0) {
    luaL_error(L, "empty table is not allowed");
  }
  luaL_checkinteger(L, -2);
  lua_remove(L, -2);
  i = (lua_type(L, -1) == LUA_TFUNCTION);
  luaL_argcheck(L, i, -1, "function expected");
}

static int luaos_finally(lua_State* L)
{
  check_function(L);
  luaos_pcall(L, 0, 0);
  if (!lua_toboolean(L, lua_upvalueindex(1))) {
    if (!lua_toboolean(L, lua_upvalueindex(3))) {
      luaos_error("%s\n", lua_tostring(L, lua_upvalueindex(2)));
    }
  }
  return 0;
}

static int luaos_catch(lua_State* L)
{
  check_function(L);
  int success = lua_toboolean(L, lua_upvalueindex(1));
  const char* errmsg = lua_tostring(L, lua_upvalueindex(2));
  if (errmsg) {
    lua_pushstring(L, errmsg);
    luaos_pcall(L, 1, 0);
  }
  lua_newtable(L);
  lua_pushboolean(L, success);
  lua_pushstring (L, errmsg ? errmsg : "");
  lua_pushboolean(L, 1);
  lua_pushcclosure(L, luaos_finally, 3);
  lua_setfield(L, -2, "finally");
  return 1;
}

static int luaos_try(lua_State* L)
{
  if (lua_gettop(L) == 0) {
    return 0;
  }
  check_function(L);
  int success = luaos_pcall(L, 0, 0);
  if (success == LUA_OK) {
    lua_pushnil(L);
  }
  lua_pushboolean(L, success == LUA_OK ? 1 : 0);

  lua_newtable(L);
  lua_pushvalue(L, 3);
  lua_pushvalue(L, 2);
  lua_pushcclosure(L, luaos_catch, 2);
  lua_setfield(L, -2, "catch");

  lua_pushvalue(L, 3);
  lua_pushvalue(L, 2);
  lua_pushcclosure(L, luaos_finally, 2);
  lua_setfield(L, -2, "finally");
  return 1;
}

/***********************************************************************************/
//lua loader
/***********************************************************************************/

static std::string cmdpath = dir::current();

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM < 502
# define loader_field "loaders"
# define lua_read(a, b, c, d) lua_load(a, b, c, d)
#else
# define loader_field "searchers"
# define lua_read(a, b, c, d) lua_load(a, b, c, d, 0)
#endif

typedef struct {
  const char* data; /* data buffer */
  size_t size;      /* data length */
} file_buffer;

bool is_slash(char c)
{
  return c == '/' || c == '\\';
}

int is_fullname(const char* filename)
{
  char c1, c2, c3;
  if (strlen(filename) < 3) {
    return 0;
  }
  c1 = filename[0];
  c2 = filename[1];
  c3 = filename[2];

  if (LUA_DIRSEP[0] == '/') { //linux
    return (c1 == '/');
  }
  if (c2 != ':' || !is_slash(c3)) {
    return 0;
  }
  return (c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z');
}

const char* skip_pathroot(const char* p)
{
  if (is_fullname(p)) {
    return p;
  }
  while (p && *p)
  {
    const char c = *p;
    if ((c == '.' && *(p + 1) != '.') || is_slash(c)) {
      p++;
      continue;
    }
    break;
  }
  return p;
}

static const char* skipBOM(const char* buff, size_t* size)
{
  const char *p = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
  for (; (*size) > 0 && *p != '\0'; buff++, (*size)--)
    if (*buff != *p++) break;
  return buff;
}

static const char* lua_reader(lua_State* L, void* ud, size_t* size)
{
  file_buffer* fb = (file_buffer*)ud;
  (void)L;  /* not used */

  if (fb->size == 0) return NULL;
  *size = fb->size;
  fb->size = 0;
  return fb->data;
}

static int lua_loader(lua_State* L, const char* buff, size_t size, const char* name)
{
  file_buffer fb;
  fb.data = buff;
  fb.size = size;
  return lua_read(L, lua_reader, &fb, name);
}

void chdir_fpath(const char* filename)
{
  static bool is_main_load = true;
  if (!is_main_load) {
    return;
  }
  is_main_load = false;
  char path[LUAOS_MAX_PATH];
  if (is_fullname(filename)) {
    strcpy(path, filename);
  }
  else {
    dir::current(path, sizeof(path));
    if (path[strlen(path) - 1] != LUA_DIRSEP[0]) {
      strcat(path, LUA_DIRSEP);
    }
    strcat(path, filename);
  }
  char* finded = strrchr(path, '/');
  if (!finded) {
    finded = strrchr(path, '\\');
    if (!finded) {
      return;
    }
  }
  if (*(finded - 1) == '.' && is_slash(*(finded - 2))) {
    finded -= 2;
  }
  *finded = 0;
  int result = chdir(path);
  dir::current(path, (int)sizeof(path));
  luaos_trace("Switch work path to: %s\n", path);
}

static int ll_fload(lua_State* L, const char* buff, size_t size, const char* filename)
{
  if (size < 4 || memcmp(buff, LUA_SIGNATURE, 4))
  {
    buff = skipBOM(buff, &size);
    if (!is_utf8(buff, size)) {
      lua_pushfstring(L, "'%s' is not utf8 encoded", filename);
      return LUA_ERRERR;
    }
    if (*buff == '#') {
      do {
        size--;
      } while (*(++buff) != '\n');
    }
  }
  char luaname[LUAOS_MAX_PATH];
  snprintf(luaname, sizeof(luaname), "@%s", filename);
  int result = lua_loader(L, buff, size, luaname);

  if (is_success(result))
    lua_pushfstring(L, "%s", luaname + 1);
  return result;
}

static int ll_fload(lua_State* L, const char* filename)
{
  lua_getglobal(L, LUA_LOADLIBNAME); /* push package */
  lua_getfield(L, -1, "readers");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return LUA_OK;
  }
  lua_remove(L, -2);  /* remove package from stack */

  int top = lua_gettop(L) - 1;
  lua_pushstring(L, filename);
  int result = lua_pcall(L, 1, LUA_MULTRET, 0);
  if (!is_success(result)) {
    return result;
  }

  if (lua_gettop(L) == top) {
    return LUA_OK;
  }

  size_t readlen = 0;
  const char* readed = luaL_checklstring(L, -1, &readlen);
  const char* romname = luaL_checkstring(L, -2);
  result = ll_fload(L, readed, readlen, filename);
  if (!is_success(result)) {
    lua_pop(L, 2);      /* remove data and romname from stack */
  }
  else {
    lua_remove(L, -3);  /* remove data from stack */
    lua_remove(L, -3);  /* remove romname from stack */
  }
  return result;
}

static void normalize(const char* filename, char* of)
{
  assert(filename && of);
  int i = 0, n = 0;
  char f[256], *x = 0;
  const char* p = filename;
  do {
    if (*p && *p != '/' && *p != '\\') {
      f[i++] = *p;
      continue;
    }
    i = f[i] = 0;
    if (strcmp(f, "..") == 0) {
      x = strrchr(of, LUA_DIRSEP[0]);
      *(x ? x : of) = 0;
      continue;
    }
    if (n++ == 0) *of = 0;
    if (strcmp(f, ".")) {
      size_t k = strlen(of);
      if (*f == 0 || (k && of[k - 1] != LUA_DIRSEP[0])) {
        strcat(of, LUA_DIRSEP);
      }
      strcat(of, f);  /* save the directory */
    }
  } while (*p++);
}

static int ll_fread(lua_State* L, const char* filefind)
{
  char filename[LUAOS_MAX_PATH];
  normalize(filefind, filename);
  int result, topidx = lua_gettop(L);
  FILE* fp = fopen(filename, "r");
  if (fp == nullptr) {
    result = ll_fload(L, filename);
  }
  else {
    char signature[4] = { 0 };
    fread(signature, 1, 4, fp);
    if (memcmp(signature, LUA_SIGNATURE, 4) == 0)
    {
      fp = freopen(filename, "rb", fp); /* reopen in binary mode */
      if (!fp) {
        return 0;
      }
    }
    size_t filesize, readed;
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    char* buffer = (char*)malloc(filesize);

    if (!buffer) {
      fclose(fp);
      return 0;
    }
    fseek(fp, 0, SEEK_SET);
    readed = fread(buffer, 1, filesize, fp);
    fclose(fp);

    chdir_fpath(filename);
    result = ll_fload(L, buffer, readed, filename);
    free(buffer);
  }
  if (!is_success(result)) {
    lua_error(L);
  }
  return lua_gettop(L) - topidx;
}

static int ll_require(lua_State* L)
{
  char filename[LUAOS_MAX_PATH];
  size_t namelen = 0;
  const char* name = luaL_checklstring(L, 1, &namelen);

  char temp[LUAOS_MAX_PATH];
  normalize(name, temp);
  name = temp;
  namelen = strlen(name);

  if (is_fullname(name))
  {
    strcpy(filename, name);
    int result = ll_fread(L, filename);
    if (result < 2) {
      strcat(filename, ".lua");
      result = ll_fread(L, filename);
    }
    return result;
  }
  for (size_t i = 0; i < namelen; i++) {
    if (temp[i] == '.') {
      temp[i] = LUA_DIRSEP[0];
    }
  }
  char path[LUAOS_MAX_PATH];
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield(L, -1, "path");
  strcpy(path, luaL_checkstring(L, -1));
  lua_pop(L, 2);

  size_t i = 0, j = 0;
  while (path[i])
  {
    const char chr = path[i++];
    if (chr == '?')
    {
      memcpy(filename + j, name, namelen);
      j += namelen;
      continue;
    }
    if (chr == ';')
    {
      filename[j] = 0;
      int result = ll_fread(L, filename);
      if (result > 0) {
        return result;
      }
      j = 0;
      continue;
    }
    filename[j++] = chr;
  }
  filename[j] = 0;
  return j ? ll_fread(L, filename) : 0;
}

static int ll_dofile(lua_State* L)
{
  int top = lua_gettop(L);
  bool replace = false;
  size_t namelen = 0;
  char name[LUAOS_MAX_PATH];
  char original[LUAOS_MAX_PATH];
  strcpy(name, luaL_checklstring(L, 1, &namelen));
  strcpy(original, name);

  for (size_t i = 0; i < namelen; i++) {
    char c = name[i];
    if (is_slash(c)) {
      name[i] = '.';
      replace = true;
    }
  }
  if (replace) {
    lua_remove(L, 1);
    lua_pushlstring(L, name, namelen);
    lua_insert(L, 1);
  }
  if (ll_require(L) < 2) {
    luaL_error(L, "file '%s' not found", original);
  }
  int result = luaos_pcall(L, 1, 0);
  if (!is_success(result)) {
    lua_error(L);
  }
  return 0;
}

static int ll_install(lua_State* L, lua_CFunction loader)
{
  lua_getglobal(L, LUA_LOADLIBNAME); /* package */
  if (loader) {
    lua_pushcfunction(L, loader);
    lua_setfield(L, -2, "readers");
  }
  lua_getfield(L, -1, loader_field); /* searchers or loaders */
  lua_pushcfunction(L, ll_require);  /* push new loader */
  lua_rawseti(L, -2, 2);
  lua_pop(L, 2);

  lua_register(L, "dofile", ll_dofile);
  return LUA_OK;
}

/***********************************************************************************/
//lua print, trace, error, throw
/***********************************************************************************/

static std::string location(lua_State* L)
{
  std::string data;
  char filename[LUAOS_MAX_PATH] = { 0 };
  for (int i = 1; i < 100; i++)
  {
    lua_Debug ar;
    int result = lua_getstack(L, i, &ar);
    if (result == 0) {
      break;
    }
    if (lua_getinfo(L, "Sl", &ar)) {
      if (ar.currentline > 0) {
        if (ar.source[0] == '@') {
          ar.source++;
        }
        sprintf(filename, "<%s:%d> ", ar.source, ar.currentline);
        data.append(filename);
        break;
      }
    }
  }
  return data;
}

static void ll_savelog(const char* data, size_t size, color_type color)
{
  std::string log(data, size);
  if (alive_exit && !alive_exit->stopped()) {
    alive_exit->post(std::bind([](const std::string& data, color_type color) {
      luaos_savelog(data, color);
    }, log, color));
  }
}

static void printf_info(const char* msg, color_type color)
{
  static std::mutex _mutex;
  std::unique_lock<std::mutex> lock(_mutex);
#ifdef _MSC_VER
  console::instance()->print(msg, color);
#else
  if (color == color_type::yellow) { //警告色(黄色)
    printf("\033[1;33m%s\033[0m", msg);
  }
  else if (color == color_type::red) { //错误色(红色)
    printf("\033[1;31m%s\033[0m", msg);
  }
  else {
    printf("\033[1;36m%s\033[0m", msg);  //缺省色(青色)
  }
#endif
  if (color != color_type::red) {
    return;
  }
  FILE* fp = fopen("~error.log", "a");
  if (fp) {
    fwrite(msg, 1, strlen(msg), fp);
    fclose(fp);
  }
}

static int ll_output(const std::string& str, color_type color)
{
  size_t size = str.size();
  const char* msg = str.c_str();

#ifdef _MSC_VER
  std::string data(str);
  if (!is_utf8(str.c_str(), str.size())) {
    data = mbs_to_utf8(str);
    msg  = data.c_str();
    size = data.size();
  }
#endif

  if (color != color_type::yellow) {
    ll_savelog(msg, size, color);
  }

  if (luaos_is_debug()) {
    printf_info(msg, color);
    return 0;
  }

  if (alive_exit && !alive_exit->stopped()) {
    std::string info(msg);
    alive_exit->post(std::bind([](const std::string& msg, color_type color) {
      printf_info(msg.c_str(), color);
    }, info, color));
  }
  return 0;
}

static int ll_printf(const std::string& str, color_type color)
{
  time_t now = time(0);
  struct tm* ptm = localtime(&now);

  char buffer[128];
  auto ms = (int)(os::milliseconds() % 1000);
  snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d,%03d] ", ptm->tm_hour, ptm->tm_min, ptm->tm_sec, ms);

  std::string data(buffer + str);
  return ll_output(data, color);
}

static int ll_printf(lua_State* L, color_type color)
{
  int count = lua_gettop(L);
  std::string strfmt(location(L));
  lua_getglobal(L, "tostring");

  for (int i = 1; i <= count; i++)
  {
    lua_pushvalue(L, -1);
    lua_pushvalue(L, i);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
      continue;
    }
    const char* str = lua_tostring(L, -1);
    if (!str) {
      continue;
    }
    strfmt.append(str);
    if (i < count) strfmt.append("\t");
    lua_pop(L, 1);
  }

  strfmt.append("\n");
  lua_pop(L, 1);
  return ll_printf(strfmt, color);
}

static int color_print(lua_State* L) {
  return ll_printf(L, color_type::normal);  /* lua print function */
}

static int color_trace(lua_State* L)
{
  return luaos_is_debug() ? ll_printf(L, color_type::yellow) : 0;
}

static int color_error(lua_State* L) {
  return ll_printf(L, color_type::red);     /* lua error function */
}

int luaos_printf(color_type color, const char* fmt, ...)
{
  std::string buffer;
  va_list va_list_args;
  va_start(va_list_args, fmt);

  size_t bytes = vsnprintf(0, 0, fmt, va_list_args);
  buffer.resize(bytes);

  va_start(va_list_args, fmt);
  vsprintf((char*)buffer.c_str(), fmt, va_list_args);
  va_end(va_list_args);
  return ll_printf(buffer, color);
}

/***********************************************************************************/

static std::mutex alive_mutex;
static std::map<lua_State*, size_t> alive_states;
static std::shared_ptr<std::thread> alive_thread;

static void interrupt(lua_State *L, lua_Debug *ar)
{
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "lua run timeout, interrupted!");
}

static void alive_check(size_t now, size_t timeout)
{
  std::unique_lock<std::mutex> lock(alive_mutex);
  for (auto iter = alive_states.begin(); iter != alive_states.end(); iter++)
  {
    lua_State* L = iter->first;
    if (lua_gethookmask(L)) {
      continue;
    }
    if (now < iter->second) {
      continue;
    }
    if (now - iter->second < timeout) {
      continue;
    }
    iter->second = 0;
    lua_sethook(L, interrupt, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
  }
}

static int keep_alive(lua_State* L)
{
  std::unique_lock<std::mutex> lock(alive_mutex);
  auto iter = alive_states.find(L);
  if (iter != alive_states.end()) {
    iter->second = os::milliseconds();
  }
  return 0;
}

static void check_thread(size_t timeout)
{
  size_t last = os::milliseconds();
  while (!alive_exit->stopped())
  {
    alive_exit->run_for(std::chrono::milliseconds(100));
    size_t now = os::milliseconds();
    if (now - last < 1000) {
      continue;
    }
    last = now;
    alive_check(now, timeout);
  }
}

static int luaos_stopped(lua_State* L)
{
  io_handler ios_local = luaos_local.lua_service();
  lua_pushboolean(L, ios_local->stopped() ? 1 : 0);
  return 1;
}

static int luaos_wait(lua_State* L)
{
  lua_State* mainL = luaos_local.lua_state();
  if (L != mainL) {
    luaL_error(L, "The wait function cannot be invoke by coroutines");
  }
  static thread_local bool notify_ok = false;
  if (!notify_ok)
  {
    notify_ok = true;
    lua_getglobal(L, luaos_waiting_name);
    if (lua_isuserdata(L, -1))
    {
      /* stop waiting */
      lua_Service* ios_wait = (lua_Service*)lua_touserdata(L, -1);
      ios_wait->stop();
      lua_pushnil(L);
      lua_setglobal(L, luaos_waiting_name);
    }
    lua_pop(L, 1);  /* remove userdata from stack */
  }
  size_t count = 0;
  size_t timeout = -1;
  if (lua_gettop(L) > 0) {
    timeout = (size_t)luaL_checkinteger(L, 1);
  }
  static bool start_ok = false;
  io_handler ios_local = luaos_local.lua_service();
  if (start_ok == false)
  {
    if (luaos_main_ios()->id() == ios_local->id()) {
      start_ok = true;
      luaos_trace("LuaOS started successfully\n");
    }
  }
  static thread_local size_t last_alive = 0;
  size_t interval = 10000;
  size_t begin = os::milliseconds();
  if (timeout == 0) {
    if (begin - last_alive > interval) {
      keep_alive(L);
      last_alive = begin;
    }
    count = ios_local->poll();
  }
  else
  {
    size_t expires = 100;
    if (timeout < 100) {
      expires = timeout;
    }
    while (!ios_local->stopped())
    {
      count += ios_local->run_for(std::chrono::milliseconds(expires));
      size_t now = os::milliseconds();
      if (now - last_alive > interval) {
        keep_alive(L);
        last_alive = now;
      }
      if (now - begin > timeout) {
        break;
      }
    }
  }
  lua_pushinteger(L, (lua_Integer)count);
  return 1;
}

static int luaos_exit(lua_State* L)
{
  io_handler ios_local = luaos_local.lua_service();
  ios_local->stop();
  return 0;
}

/***********************************************************************************/

template<typename _Ty>
inline _Ty* check_type(lua_State* L, const char* name)
{
  return lexget_userdata<_Ty>(L, 1, name);
}

struct luaos_job final {
  inline ~luaos_job() {
    stop();
  }
  inline void stop() {
    if (ios && !ios->stopped()) {
      if (status == LUA_OK) {
        auto wait = luaos_ionew();
        ios->post([wait]() { wait->stop(); });
        wait->run();
      }
      ios->stop();
    }
    if (thread && thread->joinable()) {
      thread->join();
    }
  }
  int pid;
  int status;
  std::string name;
  io_handler  ios;
  std::shared_ptr<std::thread> thread;
};

struct luaos_timer final {
  bool closed = false;
  asio::steady_timer steady;
  inline luaos_timer(io_handler ios) : steady(*ios) { }
  virtual ~luaos_timer() { }
};

static int system_clock(lua_State* L)
{
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
    ).count();
  lua_pushinteger(L, (lua_Integer)now);
  return 1;
}

static int steady_clock(lua_State* L)
{
  size_t now = os::milliseconds();
  lua_pushinteger(L, (lua_Integer)now);
  return 1;
}

static int os_typename(lua_State* L)
{
#if defined(os_windows)
  lua_pushstring(L, "windows");
#elif defined(os_bsdx)
  lua_pushstring(L, "bsd");
#elif defined(os_linux)
  lua_pushstring(L, "linux");
#elif defined(os_macx) || defined(os_mac9)
  lua_pushstring(L, "apple");
#endif
  return 1;
}

static unsigned short csum(unsigned char *data, int size)
{
  register long sum = 0;
  while (size > 1)  {
    sum += *(unsigned short*)data++;
    size -= 2;
  }
  if (size > 0) {
    sum += *(unsigned char*)data;
  }
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  return (unsigned short)~sum;
}

static int os_snowid(lua_State* L)
{
  static std::atomic<int> index(0);
  static const size_t utc2020 = 1577808000;
  static thread_local size_t tm_last = 0;
  static thread_local size_t localid = 0;

  localid = luaL_optinteger(L, 1, 0);
  if (localid < 0 || localid > 0xffff) {
    luaL_error(L, "range: 0 - 65535");
  }
  size_t tm_now = os::seconds();
  if (tm_now < tm_last) {
    tm_now = tm_last;
  }
  else if (tm_now > tm_last) {
    tm_last = tm_now;
  }
  index = ++index & 0xffff;
  size_t id = (tm_now - utc2020) & 0x7fffffff;
  id <<= 32;
  id |= ((localid & 0xffff) << 16);
  lua_pushinteger(L, (lua_Integer)(id | index));
  return 1;
}

static int os_mkdir(lua_State* L)
{
  const char* dir = luaL_checkstring(L, 1);
  bool result = dir::make(dir);
  lua_pushboolean(L, result ? 1 : 0);
  return 1;
}

static int os_pwd(lua_State* L)
{
  lua_pushlstring(L, cmdpath.c_str(), cmdpath.size());
  return 1;
}

static int os_chdir(lua_State* L)
{
  int result = chdir(luaL_checkstring(L, 1));
  if (result == 0) {
    char path[LUAOS_MAX_PATH];
    dir::current(path, (int)sizeof(path));
    luaos_trace("Switch work path to: %s\n", path);
  }
  lua_pushboolean(L, result == 0 ? 1 : 0);
  return 1;
}

static int os_id(lua_State* L)
{
  lua_pushinteger(L, luaos_local.get_id());
  return 1;
}

static int os_pid(lua_State* L)
{
  lua_pushinteger(L, luaos_local.get_pid());
  return 1;
}

static int check_utf8(lua_State* L)
{
  size_t size = 0;
  const char* data = luaL_checklstring(L, 1, &size);
  lua_pushboolean(L, is_utf8(data, size) ? 1 : 0);
  return 1;
}

static int string_split(lua_State *L)
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

template <typename Handler>
bool dir_eachof(const char* dir, const char* ext, Handler handler)
{
  _tinydir_char_t fdir[LUAOS_MAX_PATH];
  _tinydir_strcpy(fdir, dir);

  size_t len = _tinydir_strlen(fdir);
  if (fdir[len - 1] == LUA_DIRSEP[0]) {
    fdir[len - 1] = 0;
  }

  tinydir_dir tdir;
  tinydir_open(&tdir, fdir);
  while (tdir.has_next)
  {
    tinydir_file file;
    tinydir_readfile(&tdir, &file);
    tinydir_next(&tdir);

    if (file.is_dir == 0) {
      if (!ext || _tinydir_strcmp(file.extension, ext) == 0) {
        if (!handler(fdir, file)) {
          return false;
        }
      }
      continue;
    }
    if (_tinydir_strcmp(file.name, ".") == 0) {
      continue;
    }
    if (_tinydir_strcmp(file.name, "..") == 0) {
      continue;
    }
    handler(fdir, file);
    char newdir[LUAOS_MAX_PATH];
    sprintf(newdir, "%s%s%s", fdir, LUA_DIRSEP, file.name);
    if (!dir_eachof(newdir, ext, handler)) {
      break;
    }
  }
  tinydir_close(&tdir);
  return true;
}

static int enum_files(lua_State* L)
{
  if (!lua_isfunction(L, 1)) {
    luaL_argerror(L, 1, "must be a function");
  }
  const char* dir = luaL_optstring(L, 2, ".");
  if (dir[0] == 0) {
    dir = ".";
  }
  const char* ext = luaL_optstring(L, 3, "*");
  if (ext[0] == 0) {
    ext = "*";
  }
  dir_eachof(dir, 0, 
    [L, ext](const char* path, const tinydir_file& file)
    {
      if (file.is_dir) {
        return true;
      }
      if (_tinydir_stricmp(ext, "*")) {
        if (_tinydir_stricmp(file.extension, ext)) {
          return true;
        }
      }
      path = skip_pathroot(path);
      std::string filename(path);
      if (!filename.empty()) filename += LUA_DIRSEP;
      filename += file.name;

      lua_pushvalue(L, 1);
      lua_pushlstring(L, filename.c_str(), filename.size());
      if (!file.extension) {
        lua_pushnil(L);
      }
      else {
        lua_pushstring(L, file.extension);
      }
      int result = luaos_pcall(L, 2, 1);
      if (!is_success(result)) {
        luaos_error("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1); //pop error from stack
        return false;
      }
      if (lua_type(L, -1) == LUA_TNIL) {
        return true;
      }
      return lua_toboolean(L, -1) ? true : false;
    }
  );
  return 0;
}

static int local_pthread(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  lua_remove(L, 1);
  int result = luaos_pexec(L, name, lua_gettop(L));
  return is_success(result) ? 0 : lua_error(L);
}

static int local_thread(luaos_job* job, lua_value_array::value_type argv, io_handler ios)
{
  lua_State* L = luaos_local.lua_state();
  job->ios = luaos_local.lua_service();
  luaos_local.set_pid(job->pid);

  lua_pushlightuserdata(L, ios.get());
  lua_setglobal(L, luaos_waiting_name);

  lua_pushcfunction(L, local_pthread);
  lua_pushstring(L, job->name.c_str());
  job->status = luaos_pcall(L, (int)argv->push(L) + 1, 0);
  if (!ios->stopped()) {
    ios->stop();
  }
  if (!job->ios->stopped()) {
    job->ios->stop();
  }
  if (!is_success(job->status)) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  return job->status;
}

static luaos_job* check_jobself(lua_State* L)
{
  return check_type<luaos_job>(L, luaos_job_name);
}

static int load_stop(lua_State* L)
{
  luaos_job* luaself = check_jobself(L);
  if (luaself) {
    luaself->stop();
  }
  return 0;
}

static int load_id(lua_State* L)
{
  luaos_job* luaself = check_jobself(L);
  if (luaself)
  {
    int id = luaself->ios->id();
    lua_pushinteger(L, id);
    return 1;
  }
  return 0;
}

static int load_stop_gc(lua_State* L)
{
  luaos_job* luaself = check_jobself(L);
  if (luaself) {
    luaself->~luaos_job();
  }
  return 0;
}

static int load_execute(lua_State* L)
{
  lua_value_array::value_type argv;
  argv = lua_value_array::create(L, 2, 0);

  auto userdata = lexnew_userdata<luaos_job>(L, luaos_job_name);
  luaos_job* newjob = new (userdata) luaos_job();

  newjob->status = LUA_OK;
  newjob->pid    = luaos_local.get_id();
  newjob->name   = luaL_checkstring(L, 1);
  io_handler ios_wait = luaos_ionew();

  newjob->thread.reset(new std::thread(std::bind(&local_thread, newjob, argv, ios_wait)));
  ios_wait->run();
  return is_success(newjob->status) ? 1 : 0;
}

static luaos_timer* check_timerself(lua_State* L) 
{
  return check_type<luaos_timer>(L, luaos_timer_name);
}

static int steady_cancel(lua_State* L)
{
  luaos_timer* luaself = check_timerself(L);
  if (luaself) {
    luaself->closed = true;
    luaself->steady.cancel();
  }
  return 0;
}

static int steady_gc(lua_State* L)
{
  luaos_timer* luaself = check_timerself(L);
  if (luaself) {
    steady_cancel(L);
    luaself->~luaos_timer();
  }
  return 0;
}

static int steady_timer(lua_State* L)
{
  size_t expires = luaL_checkinteger(L, 1);
  int index = luaL_ref(L, LUA_REGISTRYINDEX);

  io_handler ios = luaos_local.lua_service();
  auto userdata = lexnew_userdata<luaos_timer>(L, luaos_timer_name);
  auto timer = new (userdata) luaos_timer(ios);

  lua_pushvalue(L, -1);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);  
  timer->steady.expires_after(
    std::chrono::milliseconds(expires)
  );
  timer->steady.async_wait([index, ref](const asio::error_code& ec)
  {
    lua_State* L = luaos_local.lua_state();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    luaos_timer* timer = (luaos_timer*)lua_touserdata(L, -1);
    if (timer && !timer->closed) {
      if (!ec) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, index);
        if (luaos_pcall(L, 0, 0) != LUA_OK) {
          luaos_error("%s\n", lua_tostring(L, -1));
          lua_pop(L, 1);
        }
      }
    }
    lua_pop(L, 1);  //pop timer from stack
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    luaL_unref(L, LUA_REGISTRYINDEX, index);
  });
  return 1;
}

static int throw_error(lua_State* L)
{
  lua_Debug ar;
  if (lua_getstack(L, 1, &ar) == 0) {
    lua_rawset(L, 1);
    return 0;
  }
  if (lua_getinfo(L, "Sln", &ar) && ar.currentline < 0) {
    lua_rawset(L, 1);
    return 0;
  }
  std::string name = luaL_checkstring(L, -2);
  if (lua_isfunction(L, -1)) {
    if (name == luaos_fmain) {
      lua_rawset(L, 1);
      return 0;
    }
  }
  return luaL_error(L, "Global variables cannot be used: %s", name.c_str());
}

static int disable_global(lua_State* L, lua_CFunction fn)
{
  lua_getglobal(L, "_G");
  lua_getfield(L, -1, "__newindex");
  if (lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 0;
  }

  lua_pop(L, 1);
  luaL_newmetatable(L, "placeholders");
  lua_pushliteral(L, "__newindex");
  lua_pushcfunction(L, fn);

  lua_rawset(L, -3);
  lua_setmetatable(L, -2);
  lua_pop(L, 1);
  return LUA_OK;
}

static int init_luapath(lua_State* L)
{
  char buffer[4096];
  lua_getglobal(L, LUA_LOADLIBNAME);

  char runpath[1024];
  dir::exedir(runpath, (int)sizeof(runpath));

  snprintf(buffer, sizeof(buffer), ".%s?.lua;.%s?%sinit.lua;%s..%sshare%slua%s%s.%s%s?.lua;%s..%sshare%slua%s%s.%s%s?%sinit.lua"
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_DIRSEP
    , runpath
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_VERSION_MAJOR
    , LUA_VERSION_MINOR
    , LUA_DIRSEP
    , runpath
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_VERSION_MAJOR
    , LUA_VERSION_MINOR
    , LUA_DIRSEP
    , LUA_DIRSEP
  );
  lua_pushstring(L, buffer);  /* push new path to stack */
  lua_setfield(L, -2, "path");

#ifdef _MSC_VER
  const char* extname = "dll";
#else
  const char* extname = "so";
#endif
  snprintf(buffer, sizeof(buffer), ".%slib%slua%s%s.%s%s?.%s;%s..%slib%slua%s%s.%s%s?.%s"
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_VERSION_MAJOR
    , LUA_VERSION_MINOR
    , LUA_DIRSEP
    , extname
    , runpath
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_DIRSEP
    , LUA_VERSION_MAJOR
    , LUA_VERSION_MINOR
    , LUA_DIRSEP
    , extname
  );
  lua_pushstring(L, buffer);  /* push new cpath to stack */
  lua_setfield(L, -2, "cpath");

  lua_pop(L, 1);  /* pop package from stack */
  return LUA_OK;
}

int luaos_close(lua_State* L)
{
  size_t remainder = 0;
  auto removeL = [&]() {
    std::unique_lock<std::mutex> lock(alive_mutex);
    size_t removed = alive_states.erase(L);
    remainder = alive_states.size();
    return removed;
  };
  if (removeL()) {
    if (remainder == 0) {
      if (alive_thread && alive_thread->joinable()) {
        alive_exit->poll();
        alive_exit->stop();
        alive_thread->detach();
        alive_thread.reset();
      }
    }
    lua_close(L);
  }
  return LUA_OK;
}

int luaos_pexec(lua_State* L, const char* filename, int n)
{
  char name[LUAOS_MAX_PATH];
  char original[LUAOS_MAX_PATH];
  normalize(filename, original);

  strcpy(name, original);
  size_t namelen = strlen(original);
  luaos_trace("Start module '%s' in protected mode\n", original);

  if (!is_fullname(name)) {
    for (size_t i = 0; i < namelen; i++) {
      if (is_slash(name[i])) {
        name[i] = '.';
      }
    }
  }

  lua_pushstring(L, name);
  lua_setglobal(L, "__name__");

  int stack_top = lua_gettop(L);
  lua_pushcfunction(L, ll_require);
  lua_pushstring(L, name);
  int result = luaos_pcall(L, 1, LUA_MULTRET);
  if (!is_success(result)) {
    lua_insert(L, -(n + 1));  /* insert error to bottom of params */
    lua_pop(L, n);  /* pop params from stack */
    return result;
  }

  int stack_now = lua_gettop(L);
  if (stack_now - stack_top < 2) {
    lua_pop(L, n);  /* pop params from stack */
    lua_pushfstring(L, "module '%s' not found", original);
    return LUA_ERRERR;
  }
  result = luaos_pcall(L, 1, 0);
  if (is_success(result))
  {
    lua_getglobal(L, luaos_fmain);
    if (lua_isfunction(L, -1)) {
      if (n > 0) 
        lua_insert(L, -(n + 1)); /* insert main to bottom of params */
      result = luaos_pcall(L, n, 0);
    }
  }
  else {
    lua_insert(L, -(n + 1));  /* insert error to bottom of params */
    lua_pop(L, n);  /* pop params from stack */
  }
  luaos_trace("module '%s' has exited\n", original);
  return result;
}

/***********************************************************************************/

static int luaopen_socket(lua_State* L)
{
  lua_socket::init_metatable(L);
  lua_socket::init_ssl_metatable(L);
  return 0;
}

static int luaopen_others(lua_State* L)
{
  rpcall::init_metatable(L);
  storage::init_metatable(L);
  subscriber::init_metatable(L);
  return 0;
}

static int luaopen_basic(lua_State* L)
{
  lua_getglobal(L, "error");  /* set lua throw function */
  lua_setglobal(L, "throw");
  luaL_Reg globals[] = {
    {"pcall",           pcall         },
    {"bind",            luaos_bind    },
    {"try",             luaos_try     },
    {"print",           color_print   },
    {"trace",           color_trace   },
    {"error",           color_error   },
    { NULL,             NULL          }
  };
  for (const luaL_Reg* libs = globals; libs->func; libs++) {
    lua_pushcfunction(L, libs->func);
    lua_setglobal(L, libs->name);
  }
  return 0;
}

static int luaopen_ldebug(lua_State* L)
{
  lua_getglobal(L, "debug");
  luaL_Reg methods[] = {
    {"traceback",     luaos_traceback },
    { NULL,           NULL            }
  };
  luaL_setfuncs(L, methods, 0);
  lua_pop(L, 1); //pop debug from stack
  return 0;
}

static int luaopen_los(lua_State* L)
{
  lua_getglobal(L, "os");
  luaL_Reg methods[] = {
    {"typename",      os_typename   },
    {"mkdir",         os_mkdir      },
    {"pwd",           os_pwd        },
    {"chdir",         os_chdir      },
    {"id",            os_id         },
    {"pid",           os_pid        },
    {"files",         enum_files    },
    {"snowid",        os_snowid     },
    {"wait",          luaos_wait    },
    {"stopped",       luaos_stopped },
    {"exit",          luaos_exit    },
    {"system_clock",  system_clock  },
    {"steady_clock",  steady_clock  },
    { NULL,           NULL          }
  };
  luaL_setfuncs(L, methods, 0);
  lua_pop(L, 1);  /* pop os from stack */
  return 0;
}

static int luaopen_lstring(lua_State* L)
{
  lua_getglobal(L, "string");
  luaL_Reg methods[] = {
    {"split",         string_split  },
    { NULL,           NULL          }
  };
  luaL_setfuncs(L, methods, 0);
  lua_pop(L, 1); //pop string from stack
  return 0;
}

static int luaopen_lutf8(lua_State* L)
{
  lua_getglobal(L, "utf8");
  if (lua_type(L, -1) == LUA_TNIL)
  {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "utf8");
  }
  luaL_Reg methods[] = {
    {"check",         check_utf8    },
    { NULL,           NULL          }
  };
  luaL_setfuncs(L, methods, 0);
  lua_pop(L, 1); //pop utf8 from stack
  return 0;
}

static int luaopen_job(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",         load_stop_gc  },
    { "id",           load_id       },
    { "stop",         load_stop     },
    { NULL,           NULL          },
  };
  lexnew_metatable(L, luaos_job_name, methods);
  lua_pop(L, 1);

  lua_getglobal(L, "os");
  if (lua_istable(L, -1)) {
    lua_pushcfunction(L, load_execute);
    lua_setfield(L, -2, "start");
  }
  lua_pop(L, 1);  /* pop os from stack */
  return 0;
}

static int luaopen_timer(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",         steady_gc     },
    { "cancel",       steady_cancel },
    { NULL,           NULL          },
  };
  lexnew_metatable(L, luaos_timer_name, methods);
  lua_pop(L, 1);

  lua_getglobal(L, "os");
  if (lua_istable(L, -1)) {
    lua_pushcfunction(L, ::steady_timer);
    lua_setfield(L, -2, "scheme");
  }
  lua_pop(L, 1);  /* pop os from stack */
  return 0;
}

int luaos_openlibs(lua_State* L)
{
  luaL_Reg modules[] = {
    {"basic",       luaopen_basic      },
    {"debug",       luaopen_ldebug     },
    {"os",          luaopen_los        },
    {"string",      luaopen_lstring    },
    {"utf8",        luaopen_lutf8      },
    {"job",         luaopen_job        },
    {"socket",      luaopen_socket     },
    {"timer",       luaopen_timer      },
    {"others",      luaopen_others     },
    { NULL,         NULL               }
  };
  for (const luaL_Reg* libs = modules; libs->func; libs++) {
    libs->func(L);
  }

  luaL_Reg preload[] = {
    { "msgpack",    luaopen_cmsgpack_safe },
    { "list",       luaopen_list       },
    { "openssl",    luaopen_openssl    },
    { "rapidjson",  luaopen_rapidjson  },
    { NULL,         NULL               }
  };
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield (L, -1, "preload");
  for (const luaL_Reg* libs = preload; libs->func; libs++) {
    lua_pushcfunction(L, libs->func);
    lua_setfield(L, -2, libs->name);
  }
  lua_pop(L, 2);
  return LUA_OK;
}

lua_State* luaos_newstate(lua_CFunction loader)
{
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  init_luapath(L);
  ll_install(L, loader);

  lua_pushboolean(L, luaos_is_debug() ? 1 : 0);
  lua_setglobal(L, "_DEBUG");
  disable_global(L, throw_error);

#if defined LUA_VERSION_NUM && LUA_VERSION_NUM >= 504 
  lua_gc(L, LUA_GCGEN, 0, 0);
#endif

  std::unique_lock<std::mutex> lock(alive_mutex);
  alive_states[L] = os::milliseconds();
  if (!alive_thread) {
    alive_exit->restart();
    alive_thread.reset(new std::thread(std::bind(check_thread, 600000)));
  }
  return L;
}

/***********************************************************************************/

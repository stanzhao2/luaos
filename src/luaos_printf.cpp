
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

#include <time.h>
#include <mutex>
#include <conv.h>

#include "luaos.h"
#include "luaos_printf.h"
#include "luaos_thread_local.h"

/*******************************************************************************/

static thread_local int this_thread_id = 0;
static const int _G_maxsize = 32 * 1024 * 1024;
static thread_local char print_buffer[8192];

static std::string make_path(const std::string& name)
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer), ".%s%s", LUA_DIRSEP, name.c_str());
  std::string path(buffer);

  dir::make(path.c_str());
  path += LUA_DIRSEP;
  path += module_fname();
  dir::make(path.c_str());
  path += LUA_DIRSEP;
  return path;
}

static void write_error(const std::string& data, struct tm* ptm)
{
  std::string path = make_path("err");
  char filename[256];
  snprintf(filename, sizeof(filename), "%s%d%02d%02d.log", path.c_str(), ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);

  FILE* fp = fopen(filename, "w");
  if (fp) {
    fwrite(data.c_str(), 1, data.size(), fp);
    fclose(fp);
  }
}

static void write_file(const std::string& data, struct tm* ptm)
{
  static int tmday = 0;
  static FILE* fp = nullptr;

  std::string filename;
  if (fp == nullptr)
  {
    std::string path = make_path("log");
    for (int i = 0; i < 1000; i++)
    {
      char name[256];
      snprintf(name, sizeof(name), "%04d%02d%02d.%03d.log", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, i);
      filename = path + name;
      fp = fopen(filename.c_str(), "r");
      if (fp == nullptr) {
        break;
      }

      fseek(fp, 0, SEEK_END);
      size_t size = ftell(fp);
      fclose(fp);

      fp = nullptr;
      if (size < _G_maxsize) {
        break;
      }
      filename.clear();
    }

    if (filename.empty()) {
      return;
    }

    fp = fopen(filename.c_str(), "a");
    if (fp == nullptr) {
      return;
    }

    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0)
    {
      const char* bom = "\xEF\xBB\xBF";
      fwrite(bom, 1, 3, fp);
    }

    tmday = ptm->tm_mday;
    fwrite("\n", 1, 1, fp);
  }

  fwrite(data.c_str(), 1, data.size(), fp);
  fflush(fp);

  if (ptm->tm_mday != tmday)
  {
    fclose(fp);
    fp = nullptr;
    return;
  }

  if (ftell(fp) >= _G_maxsize)
  {
    fclose(fp);
    fp = nullptr;
  }
}

static void async_printf(color_type type_color, const std::string& data)
{
#if defined(OS_WINDOWS)

  static std::mutex _mutex;
  static WORD default_color = -1;
  static HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

  std::unique_lock<std::mutex> lock(_mutex);
  if (default_color < 0)
  {
    CONSOLE_SCREEN_BUFFER_INFO defaultScreenInfo;
    GetConsoleScreenBufferInfo(out, &defaultScreenInfo);
    default_color = defaultScreenInfo.wAttributes;
  }

  WORD color = FOREGROUND_GREEN | FOREGROUND_BLUE;
  if (type_color == color_type::yellow) {
    color = FOREGROUND_RED | FOREGROUND_GREEN;
  }
  else if (type_color == color_type::red) {
    color = FOREGROUND_RED;
  }
  color |= FOREGROUND_INTENSITY;

  std::string temp(data);
  if (is_utf8(data.c_str(), data.size())) {
    temp = utf8_to_mbs(data);
  }

  SetConsoleTextAttribute(out, color);
  printf(temp.c_str());
  SetConsoleTextAttribute(out, default_color);

#else

  if (type_color == color_type::yellow) { //警告色(黄色)
    printf("\033[1;33m%s\033[0m", data.c_str());
  }
  else if (type_color == color_type::red) { //错误色(红色)
    printf("\033[1;31m%s\033[0m", data.c_str());
  }
  else {
    printf("\033[1;36m%s\033[0m", data.c_str());  //缺省色(青色)
  }

#endif
}

static void os_printf(color_type type_color, bool prefix, std::string& data)
{
  static std::mutex _mutex;

  if (this_thread_id == 0) {
    auto ios = this_thread().lua_reactor();
    this_thread_id = ios->id();
  }
  const char* type = "print";
  if (type_color == color_type::yellow) {
    type = "trace";
  }
  else if (type_color == color_type::red) {
    type = "error";
  }

  time_t now = time(0);
  struct tm* ptm = localtime(&now);
  if (prefix)
  {
    char buffer[128];
    auto ms = (int)(os::milliseconds() % 1000);
    snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d,%03d %s] <%s> ", ptm->tm_hour, ptm->tm_min, ptm->tm_sec, ms, module_fname(), type);
    data = buffer + data;
  }

  std::unique_lock<std::mutex> lock(_mutex);
  write_file(data, ptm);
  if (type_color == color_type::red) {
    write_error(data, ptm);
  }

  reactor_type ios = check_ios();
  ios->post([type_color, data]() { async_printf(type_color, data); });
}

static int lua_printf(lua_State* L, color_type color)
{
  char filename[1024] = { 0 };
  if (is_debug_mode(L) || color == color_type::red)
  {
    for (int i = 1; i < 100; i++)
    {
      lua_Debug ar;
      int result = lua_getstack(L, i, &ar);
      if (result == 0) {
        break;
      }
      if (lua_getinfo(L, "Sl", &ar))
      {
        if (ar.currentline > 0){
          sprintf(filename, "- %s:%d - ", ar.short_src, ar.currentline);
          break;
        }
      }
    }
  }
  else if (color == color_type::yellow) {
    return 0;
  }

  std::string data;
  if (filename[0]) {
    data.append(filename);
  }

  int count = lua_gettop(L);
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

    data.append(str);
    if (i < count) {
      data.append("\t");
    }
    lua_pop(L, 1);
  }

  data.append("\n");
  os_printf(color, true, data);
  lua_pop(L, 1);
  return 0;
}

/*******************************************************************************/

LUALIB_API int lua_os_print(lua_State* L)
{
  return lua_printf(L, color_type::normal);
}

LUALIB_API int lua_os_error(lua_State* L)
{
  return lua_printf(L, color_type::red);
}

LUALIB_API int lua_os_trace(lua_State* L)
{
  return lua_printf(L, color_type::yellow);
}

void my_printf(color_type type_color, bool prefix, const char* fmt, ...)
{
  static thread_local std::string buffer;

  va_list va_list_args;
  va_start(va_list_args, fmt);
  size_t bytes = vsnprintf(0, 0, fmt, va_list_args);

  if (bytes > buffer.size()) {
    buffer.resize(bytes);
  }

  va_start(va_list_args, fmt);
  vsprintf((char*)buffer.c_str(), fmt, va_list_args);
  va_end(va_list_args);

  os_printf(type_color, prefix, buffer);
  buffer.clear();
}

/*******************************************************************************/

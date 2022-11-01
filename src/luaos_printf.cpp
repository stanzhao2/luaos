
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

static void write_error(const char* data, struct tm* ptm)
{
  dir::make("./err");
  char filename[256];
  snprintf(filename, sizeof(filename), "./err/%d%02d%02d.log", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);

  FILE* fp = fopen(filename, "w");
  if (fp) {
    fwrite(data, 1, strlen(data), fp);
    fclose(fp);
  }
}

static void write_file(const char* data, struct tm* ptm)
{
  static int tmday = 0;
  static FILE* fp = nullptr;

  std::string filename;
  if (fp == nullptr)
  {
#ifdef OS_WINDOWS
    dir::make(".\\log");
    std::string path(".\\log\\");
#else
    dir::make("./log");
    std::string path("./log/");
#endif

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

  fwrite(data, 1, strlen(data), fp);
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

static void os_printf(color_type type_color, bool prefix,  const char* data)
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

  std::unique_lock<std::mutex> lock(_mutex);

  time_t now = time(0);
  struct tm* ptm = localtime(&now);
  if (prefix)
  {
    char buffer[8192];
    auto ms = (int)(os::milliseconds() % 1000);
    snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d,%03d #%02d] <%s> %s", ptm->tm_hour, ptm->tm_min, ptm->tm_sec, ms, this_thread_id, type, data);
    data = buffer;
  }
  write_file(data, ptm);
  if (type_color == color_type::red) {
    write_error(data, ptm);
  }

#if defined(OS_WINDOWS)

  static HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO defaultScreenInfo;
  GetConsoleScreenBufferInfo(out, &defaultScreenInfo);

  WORD color = FOREGROUND_GREEN | FOREGROUND_BLUE;
  if (type_color == color_type::yellow) {
    color = FOREGROUND_RED | FOREGROUND_GREEN;
  }
  else if (type_color == color_type::red) {
    color = FOREGROUND_RED;
  }

  color |= FOREGROUND_INTENSITY;
  SetConsoleTextAttribute(out, color);

  std::string mbs(data);
  if (is_utf8(mbs.c_str(), mbs.size())) {
    mbs = utf8_to_mbs(mbs);
  }

  printf("%s", mbs.c_str());
  SetConsoleTextAttribute(out, defaultScreenInfo.wAttributes);

#else

  if (type_color == color_type::yellow) { //警告色(黄色)
    printf("\033[1;33m%s\033[0m", data);
  }
  else if (type_color == color_type::red) { //错误色(红色)
    printf("\033[1;31m%s\033[0m", data);
  }
  else {
    printf("\033[1;36m%s\033[0m", data);  //缺省色(青色)
  }

#endif
}

static int lua_printf(lua_State* L, color_type color)
{
  char filename[1024] = { 0 };
  if (is_debug_mode(L))
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
          sprintf(filename, "<%s:%d> ", ar.short_src, ar.currentline);
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
  os_printf(color, true, data.c_str());
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
  char buffer[8192];
  char* output = buffer;

  va_list va_list_args;
  va_start(va_list_args, fmt);
  size_t bytes = vsnprintf(0, 0, fmt, va_list_args);

  bytes += 2;
  if (bytes > sizeof(buffer)) {
    output = (char*)malloc(bytes);
  }

  if (output)
  {
    va_start(va_list_args, fmt);
    vsprintf(output, fmt, va_list_args);
    va_end(va_list_args);

    os_printf(type_color, prefix, output);
    if (output != buffer) {
      free(output);
    }
  }
}

/*******************************************************************************/

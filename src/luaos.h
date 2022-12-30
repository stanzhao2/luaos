
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

#pragma once

#include <os/os.h>
#include <string>
#include <tinydir.h>

#include "luaset.h"
#include "luaos_printf.h"
#include "luaos_twheel.h"

#define LUAOS_VERSION "1.0.1"

/*******************************************************************************/

inline static void luaos_pop_error(lua_State* L, const char* name) 
{
  if (lua_type(L, -1) != LUA_TSTRING) {
    return;
  }
  const char* strerr = luaL_checkstring(L, -1);
  _printf(color_type::red, true, "%s runtime error: %s\n", name, strerr);
  lua_pop(L, 1);
}

template <typename Handler>
inline void dir_eachof(const char* dir, const char* ext, Handler handler)
{
  _tinydir_char_t fdir[_TINYDIR_PATH_MAX];
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

    if (file.is_dir == 0)
    {
      if (!ext || _tinydir_strcmp(file.extension, ext) == 0) {
        handler(fdir, file);
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

    char newdir[_TINYDIR_PATH_MAX];
    sprintf(newdir, "%s%s%s", fdir, LUA_DIRSEP, file.name);
    dir_eachof(newdir, ext, handler);
  }
  tinydir_close(&tdir);
}

/*******************************************************************************/

LUALIB_API int lua_os_files(lua_State* L);

LUALIB_API int lua_os_id(lua_State* L);

LUALIB_API int lua_os_deadline(lua_State* L);

LUALIB_API int lua_os_typename(lua_State* L);

LUALIB_API int lua_is_utf8(lua_State* L);

LUALIB_API int lua_traceback(lua_State* L);

LUALIB_API int lua_pcall_error(lua_State* L);

/*******************************************************************************/

lua_State* new_lua_state();

void print_copyright();

void post_alive(lua_State* L);

void post_alive_exit(lua_State* L, twheel_t *tw);

bool is_debug_mode(lua_State* L);

bool check_bom_header();

bool lua_compile(const char* romname);

bool lua_decode(const char* filename, const char* romname, std::string& data);

/*******************************************************************************/

class stack_checker final {
  int _topidx = 0;
  bool enable = true;
  lua_State* ls = 0;
public:
  inline stack_checker(lua_State* L) : ls(L) { _topidx = lua_gettop(L); }
  inline ~stack_checker() { lua_settop(ls, _topidx); }
};

/*******************************************************************************/

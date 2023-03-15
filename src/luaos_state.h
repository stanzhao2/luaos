
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

#pragma once

#include <lua_wrapper.h>
#include "luaos_color.h"

#ifndef _MSC_VER
#include <cstdarg>
#endif

#define luaos_fmain         "main"
#define luaos_waiting_name  "luaos_starting"
#define luaos_job_name      "luaos_job"
#define luaos_timer_name    "luaos_timer"

/***********************************************************************************/

bool is_slash(char c);
int  is_fullname(const char* filename);
const char* skip_pathroot(const char* p);

lua_State* luaos_newstate(lua_CFunction loader);
int luaos_openlibs(lua_State* L);
int luaos_pcall   (lua_State* L, int n, int r);
int luaos_pexec   (lua_State* L, const char* filename, int n);
int luaos_close   (lua_State* L);
int luaos_printf  (color_type color, const char* fmt, ...);

/***********************************************************************************/

#ifndef luaos_print
#define luaos_print(fmt, ...) \
luaos_printf(color_type::normal, fmt, ##__VA_ARGS__);
#endif

#ifndef luaos_trace
#define luaos_trace(fmt, ...) \
luaos_printf(color_type::yellow, fmt, ##__VA_ARGS__);
#endif

#ifndef luaos_error
#define luaos_error(fmt, ...) \
luaos_printf(color_type::red,    fmt, ##__VA_ARGS__);
#endif

/***********************************************************************************/

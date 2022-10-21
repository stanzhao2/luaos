
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

#include <lua_wrapper.h>

/*******************************************************************************/

#ifdef  _MSC_VER
#define OS_WINDOWS
#endif

#ifdef  OS_WINDOWS
#define OS_LIB "dll"
#else
#define OS_LIB "so"
#endif

#ifndef LUAOS_NAME
#define LUAOS_NAME "LuaOS"
#endif

#ifndef LUAOS_MAIN
#define LUAOS_MAIN "main"
#endif

#ifndef LUAOS_FILE_PATH
#define LUAOS_FILE_PATH "." LUA_DIRSEP "?.lua;." LUA_DIRSEP "?" LUA_DIRSEP "init.lua;." LUA_DIRSEP "lua" LUA_DIRSEP "?.lua;." LUA_DIRSEP "lua" LUA_DIRSEP "?" LUA_DIRSEP "init.lua;." LUA_DIRSEP "usr" LUA_DIRSEP "?.lua;." LUA_DIRSEP "usr" LUA_DIRSEP "?" LUA_DIRSEP "init.lua"
#endif

#ifndef LUAOS_ROOT_PATH
#define LUAOS_ROOT_PATH "." LUA_DIRSEP
#endif

#ifndef LUAOS_LIBS_PATH
#define LUAOS_LIBS_PATH "." LUA_DIRSEP "ext" LUA_DIRSEP "?." OS_LIB ";." LUA_DIRSEP "sys" LUA_DIRSEP "?." OS_LIB
#endif

/*******************************************************************************/

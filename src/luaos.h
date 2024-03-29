
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

/***********************************************************************************/

#include <tinydir.h>
#include "luaos_local.h"
#include "luaos_value.h"

#define LUAOS_MAX_PATH _TINYDIR_PATH_MAX

#define LOS_VERSION_MAJOR 2
#define LOS_VERSION_MINOR	0

class stack_rollback final {
  int _topidx = 0;
  lua_State* ls = 0;

public:
  inline stack_rollback(lua_State* L) : ls(L) { _topidx = lua_gettop(L); }
  inline ~stack_rollback() { lua_settop(ls, _topidx); }
};

bool luaos_is_debug();
bool luaos_is_leaks();
void luaos_savelog(const std::string& data, color_type color);

/***********************************************************************************/

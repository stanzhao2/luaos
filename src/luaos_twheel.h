
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

#include "twheel/twheel.h"
#include "luaos_thread_local.h"

#define TWHEEL_NAME "timingwheel"

twheel_t* this_timingwheel();

LUALIB_API int lua_os_twheel_create   (lua_State* L);
LUALIB_API int lua_os_twheel_remove   (lua_State* L);
LUALIB_API int lua_os_twheel_add_time (lua_State* L);
LUALIB_API int lua_os_twheel_max_delay(lua_State* L);
LUALIB_API int lua_os_twheel_release  (lua_State* L);

namespace lua_twheel
{
  void init_metatable(lua_State* L);
}

/***********************************************************************************/


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

#include "luaset.h"
#include "luaos_twheel.h"
#include <socket/socket.hpp>

using namespace eth;

/*******************************************************************************/

reactor_type check_ios();

class __this_thread final
{
  reactor_type  _ios;
  lua_State*    L;
  steady_timer* _timer;
  twheel_t*     _twheel;
  void on_timer(const error_code& ec, int interval);

public:
  __this_thread();
  ~__this_thread();
  void set_timer(int interval);
  lua_State* lua_state() const;
  twheel_t*  lua_twheel() const;
  reactor::ref lua_reactor() const;
};

__this_thread& this_thread();

/*******************************************************************************/

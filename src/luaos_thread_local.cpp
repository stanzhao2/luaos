
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

#include "luaos.h"
#include "luaos_thread_local.h"

/*******************************************************************************/

static const identifier hold_id;
static reactor_type __check_ios = reactor::create();
static thread_local std::shared_ptr<__this_thread> __pthread;

reactor_type check_ios()
{
  return __check_ios;
}

void __this_thread::on_timer(const error_code& ec, int interval)
{
  static thread_local size_t lastpost = 0;
  size_t now = os::milliseconds();

  if (_twheel) {
    if (_twheel->need_update) {
      timewheel_update(_twheel, now);
    }
  }
  if (now - lastpost >= 1000)
  {
    lastpost = now;
    post_alive(L);
  }
  if (!ec) set_timer(interval);
}

__this_thread& this_thread() {
  if (!__pthread) {
    __pthread = std::make_shared<__this_thread>();
  }
  return *__pthread;
}

void __this_thread::set_timer(int interval)
{
  if (!_timer) {
    _timer = new steady_timer(*_ios);
  }
  _timer->expires_after(
    std::chrono::milliseconds(interval)
  );
  _timer->async_wait(
    std::bind(&__this_thread::on_timer, this, std::placeholders::_1, interval)
  );
}

__this_thread::__this_thread()
  : _timer(0)
{
  L = luaL_newstate();
  post_alive(L);

  _twheel = timewheel_create(0);
  _ios = reactor::create();
  _ios->post([this]() { set_timer(10); });
}

__this_thread::~__this_thread()
{
  if (L) {
    post_alive_exit(L, _twheel);
    L = nullptr;
    _twheel = nullptr;
  }
  if (_timer) delete _timer;
}

lua_State* __this_thread::lua_state() const {
  return L;
}

twheel_t* __this_thread::lua_twheel() const {
  return _twheel;
}

reactor::ref __this_thread::lua_reactor() const {
  return _ios;
}

/*******************************************************************************/


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

#include "luaos.h"
#include "luaos_compile.h"

/***********************************************************************************/

static io_handler main_handler;

/***********************************************************************************/

static int luaos_loader(lua_State* L)
{
  return luaos_loadlua(L, luaL_checkstring(L, 1));
}

static void luaos_signal(int code)
{
  signal(code, luaos_signal);
  if (code == SIGINT) {
    printf("\n");
  }
  if (main_handler && !main_handler->stopped()) {
    main_handler->stop();
  }
}

/***********************************************************************************/

io_handler luaos_main_ios()
{
  return main_handler;
}

local_values::local_values()
{
  _pid = 0;
  _ios = luaos_ionew();
  if (main_handler == nullptr)
  {
    main_handler = _ios;
    signal(SIGINT,  luaos_signal);
    signal(SIGTERM, luaos_signal);
  }
  _L = luaos_newstate(luaos_loader);
  luaos_openlibs(_L);
}

local_values::~local_values()
{
  luaos_close(_L);
}

local_values& local_values::instance()
{
  static thread_local local_values _instance;
  return _instance;
}

/***********************************************************************************/

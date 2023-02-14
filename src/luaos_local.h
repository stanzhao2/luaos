
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

#include "luaos_io.h"
#include "luaos_state.h"

/***********************************************************************************/

class local_values final {
  local_values();
  lua_State* _L;
  io_handler _ios;

public:
  static local_values& instance();
  inline lua_State*  lua_state() const { return _L; }
  inline io_handler  lua_service() const { return _ios; }
  virtual ~local_values();
};

io_handler luaos_main_ios();

#define luaos_local local_values::instance()

/***********************************************************************************/

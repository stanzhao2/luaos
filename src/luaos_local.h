
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

#include <map>
#include "luaos_io.h"
#include "luaos_state.h"

/***********************************************************************************/

typedef struct {
  int type;
  size_t tmms;
  size_t count, size;
} mem_trunk;

typedef std::map<std::string, mem_trunk> memused_type;
typedef std::map<void*, std::string> memaddr_type;

class local_values final {
  local_values();
  int _pid;
  lua_State* _L;
  io_handler _ios;
  memused_type mem_used;
  memaddr_type mem_address;

public:
  static local_values& instance();
  inline int  get_id()  const { return _ios->id(); }
  inline int  get_pid() const { return _pid; }
  inline void set_pid(int id) { _pid = id; }
  inline lua_State*  lua_state() const { return _L; }
  inline io_handler  lua_service() const { return _ios; }
  inline memused_type& mused() { return mem_used; }
  inline memaddr_type& maddr() { return mem_address; }
  virtual ~local_values();
};

io_handler luaos_main_ios();
lua_State* alloc_new_state(void* ud);

#define luaos_local local_values::instance()
#define local_used local_values::instance().mused()
#define local_used_address local_values::instance().maddr()

/***********************************************************************************/

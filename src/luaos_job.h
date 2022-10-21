
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

#include <thread>
#include <vector>
#include <string>
#include "luaos_thread_local.h"

/*******************************************************************************/

class lua_job final {
public:
  typedef std::vector<std::any> job_params;
  void  stop();
  int   id() const;
  bool  run(const char* name, const job_params& argvs);
  const char* name() const;
  virtual ~lua_job();

public:
  static const char* metatable_name();
  static void init_metatable(lua_State* L);
  static lua_job** check_metatable(lua_State* L);

private:
  reactor::ref _ios;
  std::string  _name;   //name of this job
  std::thread* _thread; //work thread
  void job_task(const job_params& argvs);
  static void job_thread(lua_job* job, const job_params& argvs);
};

/*******************************************************************************/

LUALIB_API int lua_os_job_name(lua_State* L);
LUALIB_API int lua_os_job_id  (lua_State* L);
LUALIB_API int lua_os_job_gc  (lua_State* L);
LUALIB_API int lua_os_job_stop(lua_State* L);

LUALIB_API int lua_os_wait   (lua_State* L);
LUALIB_API int lua_os_stopped(lua_State* L);
LUALIB_API int lua_os_exit   (lua_State* L);
LUALIB_API int lua_os_execute(lua_State* L);

/*******************************************************************************/

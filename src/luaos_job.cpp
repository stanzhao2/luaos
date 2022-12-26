
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

#include <functional>
#include <atomic>
#include <tinydir.h>

#include "luaos.h"
#include "luaos_loader.h"
#include "luaos_job.h"

static const identifier hold_id;
static std::atomic<bool> _ready(false);
static std::atomic<bool> _success(false);

/*******************************************************************************/

lua_job::~lua_job()
{
  stop();
}

void lua_job::job_task(const job_params& params)
{
  _ios = this_thread().lua_reactor();
  lua_State* L = new_lua_state();
  if (!L) {
    return;
  }

  _printf(color_type::yellow, true, "loading %s module...\n", name());
  if (lua_dofile(L, name()) != LUA_OK) {
    luaos_pop_error(L, name());
    return;
  }

  lua_pushcfunction(L, lua_pcall_error);
  int error_fn_index = lua_gettop(L);
  lua_getglobal(L, LUAOS_MAIN);

  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2); //pop nil and lua_pcall_error from stack
    return;
  }

  for (size_t i = 0; i < params.size(); i++) {
    lexpush_any(L, params[i]);
  }

  lua_pcall(L, (int)params.size(), 0, error_fn_index);
  lua_pop(L, lua_gettop(L));
}

bool lua_job::run(const char* name, const job_params& params)
{
  _ready  = _success = false;
  _ios    = 0;
  _name   = name;
  _thread = new std::thread(std::bind(job_thread, this, params));

  if (_thread)
  {
    while (!_ready)
    {
      std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
      );
    }

    if (!_success && _thread->joinable())
    {
      _thread->join();
      delete _thread;
      _thread = 0;
    }
  }
  return _success;
}

void lua_job::job_thread(lua_job* job, const job_params& argvs)
{
  job->job_task(argvs);
  _ready = true;
  _printf(color_type::yellow, true, "module %s has exited\n", job->name());
}

void lua_job::stop()
{
  assert(_ios);
  if (!_thread) {
    return;
  }

  if (_thread->joinable())
  {
    _ios->stop();
    _thread->join();
  }

  delete _thread;
  _thread = 0;
}

const char* lua_job::name() const
{
  return _name.c_str();
}

int lua_job::id() const
{
  return _ios->id();
}

const char* lua_job::metatable_name()
{
  return "luaos-job";
}

lua_job** lua_job::check_metatable(lua_State* L)
{
  lua_job** self = lexget_userdata<lua_job*>(L, 1, metatable_name());
  if (!self || !(*self)) {
    return nullptr;
  }
  return self;
}

void lua_job::init_metatable(lua_State* L)
{
  struct luaL_Reg functions[] = {
    { "execute",  lua_os_execute   },
    { "exit",     lua_os_exit },
    { "wait",     lua_os_wait },
    { "stopped",  lua_os_stopped },
    { NULL,       NULL },
  };

  lua_getglobal(L, "os");
  luaL_setfuncs(L, functions, 0);
  lua_pop(L, 1); //pop os from stack

  struct luaL_Reg methods[] = {
    { "__gc",     lua_os_job_gc    },
    //{ "__close",  lua_os_job_stop  },
    { "stop",     lua_os_job_stop  },
    { "id",       lua_os_job_id    },
    { "name",     lua_os_job_name  },
    { NULL,       NULL },
  };
  lexnew_metatable(L, metatable_name(), methods);
  lua_pop(L, 1);
}

/*******************************************************************************/

LUALIB_API int lua_os_job_name(lua_State* L)
{
  lua_job** mt = lua_job::check_metatable(L);
  if (mt) {
    lua_job* job = *mt;
    lua_pushstring(L, job->name());
  }
  return mt ? 1 : 0;
}

LUALIB_API int lua_os_job_id(lua_State* L)
{
  lua_job** mt = lua_job::check_metatable(L);
  if (mt) {
    lua_job* job = *mt;
    lua_pushinteger(L, job->id());
  }
  return mt ? 1 : 0;
}

LUALIB_API int lua_os_job_stop(lua_State* L)
{
  lua_job** mt = lua_job::check_metatable(L);
  if (mt)
  {
    lua_job* job = *mt;
    job->stop();
  }
  return 0;
}

LUALIB_API int lua_os_job_gc(lua_State* L)
{
  lua_job** mt = lua_job::check_metatable(L);
  if (mt)
  {
    lua_job* job = *mt;
    delete job;
    *mt = 0;
  }
  return 0;
}

LUALIB_API int lua_os_exit(lua_State* L)
{
  auto ios = this_thread().lua_reactor();
  ios->stop();
  return 0;
}

LUALIB_API int lua_os_stopped(lua_State* L)
{
  auto ios = this_thread().lua_reactor();
  lua_pushboolean(L, ios->stopped() ? 1 : 0);
  return 1;
}

LUALIB_API int lua_os_wait(lua_State* L)
{
  _ready = _success = true;
  size_t runcnt = 0;
  size_t timeout = luaL_optinteger(L, 1, -1);

  auto ios = this_thread().lua_reactor();
  if (timeout == -1) {
    runcnt = ios->run();
  }
  else if (timeout == 0) {
    runcnt = ios->poll();
  }
  else {
    runcnt = ios->run_for(std::chrono::milliseconds(timeout));
  }

  lua_pushinteger(L, (lua_Integer)runcnt);
  return 1;
}

LUALIB_API int lua_os_execute(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  if (_tinydir_stricmp(name, LUAOS_MAIN) == 0) {
    return 0;
  }

  int argc = lua_gettop(L);
  lua_job::job_params params;
  for (int i = 2; i <= argc; i++) {
    params.push_back(lexget_any(L, i));
  }

  lua_job* job = new lua_job();
  if (!job) {
    return 0;
  }

  if (!job->run(name, params))
  {
    job->stop();
    delete job;
    return 0;
  }

  lua_job** userdata = lexnew_userdata<lua_job*>(L, lua_job::metatable_name());
  if (userdata) {
    *userdata = job;
  }
  return userdata ? 1 : 0;
}

/*******************************************************************************/

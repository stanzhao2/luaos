
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

/********************************************************************************/

static thread_local lua_State* GL = 0;
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon  (void *ud, const char *message, int tocont);
static void warnfcont(void *ud, const char *message, int tocont);

static int panic(lua_State *L) {
  const char *msg = lua_tostring(L, -1);
  if (msg == NULL) {
    msg = "error object is not a string";
  }
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n", msg);
  return 0;
}

static int checkcontrol(lua_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      lua_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      lua_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}

static void warnfcont(void *ud, const char *message, int tocont) {
  lua_State *L = (lua_State *)ud;
  lua_writestringerror("%s", message);  /* write message */
  if (tocont) { /* not the last part? */
    lua_setwarnf(L, warnfcont, L);  /* to be continued */
  }
  else {  /* last part */
    lua_writestringerror("%s", "\n");  /* finish message with end-of-line */
    lua_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}

static void warnfon(void *ud, const char *message, int tocont) {
  if (checkcontrol((lua_State*)ud, message, tocont)) { /* control message? */
    return;  /* nothing else to be done */
  }
  lua_writestringerror("%s", "Lua warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}

static void warnfoff(void *ud, const char *message, int tocont) {
  checkcontrol((lua_State *)ud, message, tocont);
}

static char* ll_stack(char *buffer, size_t size) {
  lua_State* L = GL;
  if (!L) {
    return 0;
  }
  lua_Debug ar;
  int result = lua_getstack(L, 0, &ar);
  if (result == 0) {
    return 0;
  }
  if (!lua_getinfo(L, "Sl", &ar)) {
    return 0;
  }
  if (ar.currentline < 0) {
    return 0;
  }
  snprintf(buffer, size, "%s:%d", ar.short_src, ar.currentline);
  return buffer;
}

static int skynet_snapshot(lua_State* L) {
  auto& mem_address = local_used_address;
  auto& mem_used = local_used;

  if (!luaos_is_debug()) {
    lua_pushboolean(L, 0);
    return 1;
  }

  const char* filename = luaL_optstring(L, 1, "snapshot.log");
  FILE* fp = fopen(filename, "w");
  if (!fp) {
    lua_pushboolean(L, 0);
    return 1;
  }

  char buffer[8192];
  auto iter = mem_used.begin();
  for (; iter != mem_used.end(); ++iter) {
    const auto& trunk = iter->second;
    snprintf(buffer, sizeof(buffer), "% 8llu\t% 8llu\t%s\n", trunk.count, trunk.size, iter->first.c_str());
    fwrite(buffer, 1, strlen(buffer), fp);
  }

  fclose(fp);
  lua_pushboolean(L, 1);
  return 1;
}

static void ll_record(const char* file, void* op, void* np, size_t osize, size_t nsize) {
  auto& mem_address = local_used_address;
  auto& mem_used = local_used;
  /* malloc */
  if (np) {
    if (op == nullptr) {
      switch (osize) {
      case LUA_TNIL:
      case LUA_TBOOLEAN:
      case LUA_TLIGHTUSERDATA:
      case LUA_TNUMBER:
      case LUA_TSTRING:
      case LUA_NUMTYPES:
        return;
      }
    }
    mem_address[np] = file;
    auto iter = mem_used.find(file);
    if (iter != mem_used.end()) {
      iter->second.count++;
      iter->second.size += nsize;
    }
    else {
      mem_trunk trunk;
      trunk.count = 1;
      trunk.size  = nsize;
      mem_used[file] = trunk;
    }
  }
  /* free */
  if (op) {
    auto pp = mem_address.find(op);
    if (pp == mem_address.end()) {
      return;
    }
    auto iter = mem_used.find(pp->second.c_str());
    mem_address.erase(pp);

    if (iter == mem_used.end()) {
      return;
    }
    auto count = iter->second.count;
    if (count > 0) {
      iter->second.count--;
      iter->second.size -= osize;
    }
    if (count == 1) {
      mem_used.erase(iter);
    }
  }
}

static void* ll_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  bool debugging = luaos_is_debug();

  /* free memory */
  if (nsize == 0) {
    if (debugging && GL) {
      ll_record(nullptr, ptr, nullptr, osize, nsize);
    }
    free(ptr);
    return NULL;
  }

  /* alloc memory */
  void* pnew = realloc(ptr, nsize);
  if (debugging && GL) {
    char name[1024];
    const char* file = ll_stack(name, sizeof(name));
    if (file) {
      ll_record(file, ptr, pnew, osize, nsize);
    }
  }
  return pnew;
}

lua_State* alloc_new_state(void* ud) {
  lua_State* L = lua_newstate(ll_alloc, ud);
  if (L) {
    luaL_checkversion(L);
    lua_atpanic(L, &panic);
    lua_setwarnf(L, warnfoff, L);  /* default is warnings off */

    lua_pushcfunction(L, skynet_snapshot);
    lua_setglobal(L, "snapshot");
  }
  return L;
}

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
  if (_L) {
    luaos_close(_L);
    GL = _L = nullptr;
  }
}

local_values& local_values::instance()
{
  static thread_local local_values _instance;
  GL = _instance._L;
  return _instance;
}

/***********************************************************************************/

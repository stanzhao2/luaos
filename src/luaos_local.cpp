
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

static thread_local lua_State* gL = 0;
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

static lua_State *getthread(lua_State *L) {
  return lua_isthread(L, 1) ? lua_tothread(L, 1) : L;
}

static char* ll_stack(lua_State* L, char *buffer, size_t size) {
  L = getthread(L);
  lua_Debug ar;
  for (int i = 0; i < 100; i++) {
    if (!lua_getstack(L, i, &ar)) {
      break;
    }
    if (!lua_getinfo(L, "Sln", &ar)) {
      break;
    }
    if (ar.currentline > 0) {
      break;
    }
  }
  if (ar.currentline < 1) {
    return nullptr;
  }
  if (ar.name && ar.namewhat) {
    snprintf(buffer, size, "%s:%d <%s %s>", ar.short_src, ar.currentline, ar.name, ar.namewhat);
  } else if (ar.what) {
    snprintf(buffer, size, "%s:%d <%s>", ar.short_src, ar.currentline, ar.what);
  } else {
    snprintf(buffer, size, "%s:%d", ar.short_src, ar.currentline);
  }
  return buffer;
}

static const char* skynet_mem_free(local_values* lua, void* ptr, size_t osize, int& luaT) {
  static thread_local std::string fline;
  memused_type& mmused = lua->mused();
  memaddr_type& mmaddr = lua->maddr();

  auto ppos = mmaddr.find(ptr);
  if (ppos == mmaddr.end()) {
    return nullptr;
  }

  fline = ppos->second;
  auto iter = mmused.find(fline);
  mmaddr.erase(ppos);

  if (iter != mmused.end()) {
    auto count = iter->second.count;
    if (count > 0) {
      luaT = iter->second.type;
      iter->second.count--;
      iter->second.size -= osize;
      if (iter->second.count == 0) {
        mmused.erase(iter);
      }
    }
  }
  return fline.c_str();
}

static  void skynet_mem_alloc(local_values* lua, int type, void* pnew, size_t nsize, const char* fline) {
  memused_type& mmused = lua->mused();
  memaddr_type& mmaddr = lua->maddr();

  switch (type) {
  case LUA_TNONE:
  case LUA_TNIL:
  case LUA_TBOOLEAN:
  case LUA_TLIGHTUSERDATA:
  case LUA_TNUMBER:
  case LUA_TSTRING:
    return;
  }
  mmaddr[pnew] = fline;
  auto iter = mmused.find(fline);
  if (iter != mmused.end()) {
    iter->second.count++;
    iter->second.size += nsize;
  }
  else {
    mem_trunk trunk;
    trunk.type  = type;
    trunk.count = 1;
    trunk.tmms  = os::milliseconds();
    trunk.size  = nsize;
    mmused[fline] = trunk;
  }
}

static void ll_on_hook(lua_State* L, lua_Debug* ar) {
  /* to do nothine */
}

static int skynet_snapshot(lua_State* L) {
  lua_gc(L, LUA_GCCOLLECT);
  memused_type& mmused = local_used;
  lua_newtable(L);
  for (auto iter = mmused.begin(); iter != mmused.end(); ++iter) {
    lua_newtable(L);
    lua_pushinteger(L, (lua_Integer)iter->second.count);
    lua_setfield(L, -2, "count");
    lua_pushinteger(L, (lua_Integer)iter->second.tmms);
    lua_setfield(L, -2, "time");
    lua_pushinteger(L, (lua_Integer)iter->second.size);
    lua_setfield(L, -2, "usage");
    const char* type_name = lua_typename(L, iter->second.type);
    lua_pushfstring(L, "[%s]", type_name);
    lua_setfield(L, -2, "typename");
    lua_setfield(L, -2, iter->first.c_str());
  }
  return 1;
}

static void* ll_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  local_values* lua = (local_values*)ud;
  /* the same size */
  if (ptr && osize == nsize) {
    return ptr;
  }

  /* free memory */
  int  luaT = 0;
  bool snapshot = luaos_is_debug();
  if (nsize == 0) {
    if (snapshot) {
      skynet_mem_free(lua, ptr, osize, luaT);
    }
    free(ptr);
    return NULL;
  }

  /* alloc memory */
  void* pnew = realloc(ptr, nsize);
  if (!snapshot || !pnew) {
    return pnew;
  }
  if (snapshot && gL){
    if (!lua_gethookmask(gL)) {
      lua_sethook(gL, ll_on_hook, LUA_MASKLINE, 0);
    }
  }

  char temp[1024];
  const char* fline = nullptr;
  if (ptr) {
    if (pnew == ptr) {
      return pnew;
    }
    fline = skynet_mem_free(lua, ptr, osize, luaT);
  }
  else if (gL) {
    fline = ll_stack(gL, temp, sizeof(temp));
    luaT = (int)osize;
  }
  if (fline && luaT) {
    skynet_mem_alloc(lua, luaT, pnew, nsize, fline);
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
  _L = luaos_newstate(luaos_loader, this);
  luaos_openlibs(_L);
}

local_values::~local_values()
{
  if (_L) {
    gL = nullptr;
    luaos_close(_L);
    _L = nullptr;
  }
}

local_values& local_values::instance()
{
  static thread_local local_values _instance;
  gL = _instance._L;
  return _instance;
}

/***********************************************************************************/

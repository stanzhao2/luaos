
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

static void ll_mark_object(lua_State *L, lua_State *dL, const void * parent, const char * desc);

#define UNKNOWN		0
#define TABLE			1
#define FUNCTION	2
#define THREAD		3
#define USERDATA	4
#define MARK			5

/********************************************************************************/

static bool ll_is_marked(lua_State* dL, const void* p) {
  lua_rawgetp(dL, MARK, p);
  if (lua_isnil(dL, -1)) {    /* if not marked */
    lua_pop(dL, 1);           /* pop nil */
    lua_pushboolean(dL, 1);   /* push true */
    lua_rawsetp(dL, MARK, p); /* set marked: t[p] = true */
    return false;
  }
  lua_pop(dL, 1); /*pop true */
  return true;
}

static bool ll_is_lightcfunction(lua_State *L, int i) {
  if (lua_iscfunction(L, i)) {
    if (lua_getupvalue(L, i, 1) == NULL) {
      return true; /* not have upvalue */
    }
    lua_pop(L, 1);
  }
  return false;
}

static int ll_checktype(lua_State *L, int i) {
  switch (lua_type(L, i)) {
  case LUA_TTABLE:
    return TABLE;
  case LUA_TFUNCTION:
    if (ll_is_lightcfunction(L, i)) break;
    return FUNCTION;
  case LUA_TTHREAD:
    return THREAD;
  case LUA_TUSERDATA:
    return USERDATA;
  }
  lua_pop(L, 1);
  return UNKNOWN;
}

static const void* ll_read_object(lua_State *L, lua_State *dL, const void *parent, const char *desc) {
  int tidx = ll_checktype(L, -1);
  if (tidx == UNKNOWN) {
    return NULL;
  }
  const void *pv = lua_topointer(L, -1);
  if (ll_is_marked(dL, pv)) {
    lua_rawgetp(dL, tidx, pv);
    if (!lua_isnil(dL, -1)) {
      lua_pushstring(dL, desc);
      lua_rawsetp(dL, -2, parent);
    }
    lua_pop(dL, 1);
    lua_pop(L,  1);
    return NULL;
  }
  lua_newtable(dL);
  lua_pushstring(dL, desc);
  lua_rawsetp(dL, -2, parent);
  lua_rawsetp(dL, tidx, pv);
  return pv;
}

static const char* ll_key_tostring(lua_State *L, int i, char * buffer, size_t size) {
  int type = lua_type(L, i);
  switch (type) {
  case LUA_TSTRING:
    return lua_tostring(L, i);
  case LUA_TNUMBER:
    snprintf(buffer, size, "[%lg]", lua_tonumber(L, i));
    break;
  case LUA_TBOOLEAN:
    snprintf(buffer, size, "[%s]", lua_toboolean(L, i) ? "true" : "false");
    break;
  case LUA_TNIL:
    snprintf(buffer, size, "[nil]");
    break;
  default:
    snprintf(buffer, size, "[%s:%p]", lua_typename(L, type), lua_topointer(L, i));
    break;
  }
  return buffer;
}

static void ll_mark_table(lua_State *L, lua_State *dL, const void * parent, const char * desc) {
  const void * pv = ll_read_object(L, dL, parent, desc);
  if (pv == NULL) {
    return;
  }
  bool weakk = false;
  bool weakv = false;
  if (lua_getmetatable(L, -1)) {
    lua_pushliteral(L, "__mode");
    lua_rawget(L, -2);
    if (lua_isstring(L,-1)) {
      const char *mode = lua_tostring(L, -1);
      if (strchr(mode, 'k')) {
        weakk = true;
      }
      if (strchr(mode, 'v')) {
        weakv = true;
      }
    }
    lua_pop(L,1);
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    ll_mark_table(L, dL, pv, "[metatable]");
  }
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    if (weakv) {
      lua_pop(L, 1);
    }
    else {
      char key[32];
      const char *desc = ll_key_tostring(L, -2, key, sizeof(key));
      ll_mark_object(L, dL, pv , desc);
    }
    if (!weakk) {
      lua_pushvalue(L, -1);
      ll_mark_object(L, dL, pv , "[key]");
    }
  }
  lua_pop(L, 1);
}

static void ll_mark_userdata(lua_State *L, lua_State *dL, const void * parent, const char *desc) {
  const void * pv = ll_read_object(L, dL, parent, desc);
  if (pv == NULL) {
    return;
  }
  if (lua_getmetatable(L, -1)) {
    ll_mark_table(L, dL, pv, "[metatable]");
  }
  lua_getuservalue(L, -1);
  if (lua_isnil(L,-1)) {
    lua_pop(L,2);
  }
  else {
    ll_mark_object(L, dL, pv, "[userdata]");
    lua_pop(L,1);
  }
}

static void ll_mark_function(lua_State *L, lua_State *dL, const void * parent, const char *desc) {
  const void * pv = ll_read_object(L, dL, parent, desc);
  if (pv == NULL) {
    return;
  }
  for (int i = 1; ; i++) {
    const char *name = lua_getupvalue(L, -1, i);
    if (name == NULL) {
      break;
    }
    ll_mark_object(L, dL, pv, name[0] ? name : "[upvalue]");
  }
  lua_pop(L,1); /* get upvalue only */
}

static void ll_mark_thread(lua_State *L, lua_State *dL, const void * parent, const char *desc) {
  const void * pv = ll_read_object(L, dL, parent, desc);
  if (pv == NULL) {
    return;
  }
  int level = 0;
  lua_State *cL = lua_tothread(L, -1);
  if (cL == L) {
    level = 1;
  }
  else {
    int top = lua_gettop(cL);
    luaL_checkstack(cL, 1, NULL);
    char tmp[16];
    for (int i = 0; i < top; i++) {
      lua_pushvalue(cL, i + 1);
      sprintf(tmp, "[%d]", i + 1);
      ll_mark_object(cL, dL, cL, tmp);
    }
  }
  lua_Debug ar;
  while (lua_getstack(cL, level, &ar)) {
    lua_getinfo(cL, "Sl", &ar);
    for (int j = 1; j > -1; j -= 2) {
      for (int i = j; ; i += j) {
        const char * name = lua_getlocal(cL, &ar, i);
        if (name == NULL) {
          break;
        }
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "%s:%s:%d", name, ar.short_src, ar.currentline);
        ll_mark_object(cL, dL, pv, tmp);
      }
    }
    ++level;
  }
  lua_pop(L, 1);
}

static void ll_mark_object(lua_State *L, lua_State *dL, const void * parent, const char *desc) {
  luaL_checkstack(L, LUA_MINSTACK, NULL);
  switch (lua_type(L, -1)) {
  case LUA_TTABLE:
    ll_mark_table(L, dL, parent, desc);
    break;
  case LUA_TUSERDATA:
    ll_mark_userdata(L, dL, parent, desc);
    break;
  case LUA_TFUNCTION:
    ll_mark_function(L, dL, parent, desc);
    break;
  case LUA_TTHREAD:
    ll_mark_thread(L, dL, parent, desc);
    break;
  default:
    lua_pop(L, 1);
    break;
  }
}

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

static char* ll_stack(lua_State* L, char *buffer, size_t size, std::string& what) {
  L = getthread(L);
  lua_Debug ar;
  for (int i = 0; i < 100; i++) {
    if (!lua_getstack(L, i, &ar)) {
      return nullptr;
    }
    if (!lua_getinfo(L, "Sln", &ar)) {
      return nullptr;
    }
    if (ar.currentline > 0 && ar.short_src) {
      break;
    }
  }
  char temp[128];
  what.clear();
  if (ar.name && ar.namewhat) {
    snprintf(temp, sizeof(temp), "%s %s", ar.name, ar.namewhat);
    what.assign(temp);
  }
  else if (ar.what) {
    what.assign(ar.what);
  }
  snprintf(buffer, size, "%s:%d", ar.short_src, ar.currentline);
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
      iter->second.pointers.erase(ptr);

      if (iter->second.count == 0) {
        mmused.erase(iter);
      }
    }
  }
  return fline.c_str();
}

static void skynet_mem_alloc(local_values* lua, int type, void* pnew, size_t nsize, const char* fline, const char* what) {
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
    iter->second.pointers.insert(pnew);
  }
  else {
    mem_trunk trunk;
    trunk.what.assign(what);
    trunk.type  = type;
    trunk.count = 1;
    trunk.tmms  = os::milliseconds();
    trunk.size  = nsize;
    trunk.pointers.insert(pnew);
    mmused[fline] = trunk;
  }
}

static void ll_on_hook(lua_State* L, lua_Debug* ar) {
  /* to do nothine */
}

static int skynet_snapshot(lua_State* L) {
  lua_gc(L, LUA_GCCOLLECT);
  lua_State *dL = luaL_newstate();
  for (int i = 0; i < MARK; i++) {
    lua_newtable(dL);
  }
  lua_pushvalue(L, LUA_REGISTRYINDEX);
  ll_mark_table(L, dL, NULL, "[registry]");

  memused_type& mmused = local_used;
  lua_createtable(L, 0, (int)mmused.size());
  for (auto iter = mmused.begin(); iter != mmused.end(); ++iter) {
    lua_newtable(L); /* files table */
    lua_newtable(L); /* leaks table */
    auto& ps = iter->second.pointers;

    for (auto addr = ps.begin(); addr != ps.end(); ++addr) {
      lua_rawgetp(dL, TABLE, *addr); /* get parent table */
      if (!lua_istable(dL, -1)) {
        lua_pop(dL, 1);
        continue;
      }
      lua_newtable(L); /* parent table */
      lua_pushnil(dL);
      while (lua_next(dL, -2)) {
        lua_pushstring(L, lua_tostring(dL, -1));
        lua_rawsetp(L, -2, lua_topointer(dL, -2));
        lua_pop(dL, 1);
      }
      lua_rawsetp(L, -2, *addr);
      lua_pop(dL, 1);	/* pop parent table */
    }
    lua_setfield(L, -2, "leaks");

    lua_pushinteger(L, (lua_Integer)iter->second.count);
    lua_setfield(L, -2, "count");

    lua_pushinteger(L, (lua_Integer)iter->second.tmms);
    lua_setfield(L, -2, "time");

    lua_pushinteger(L, (lua_Integer)iter->second.size);
    lua_setfield(L, -2, "usage");

    lua_pushstring(L, iter->second.what.c_str());
    lua_setfield(L, -2, "what");

    lua_pushfstring(L, "%s", lua_typename(L, iter->second.type));
    lua_setfield(L, -2, "typename");
    lua_setfield(L, -2, iter->first.c_str());
  }
  lua_close(dL);
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
  std::string what;
  if (ptr) {
    if (pnew == ptr) {
      return pnew;
    }
    fline = skynet_mem_free(lua, ptr, osize, luaT);
  }
  else if (gL) {
    fline = ll_stack(gL, temp, sizeof(temp), what);
    luaT = (int)osize;
  }
  if (fline && luaT) {
    skynet_mem_alloc(lua, luaT, pnew, nsize, fline, what.c_str());
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

static int luaos_loader(lua_State* L) {
  return luaos_loadlua(L, luaL_checkstring(L, 1));
}

static void luaos_signal(int code) {
  signal(code, luaos_signal);
  if (code == SIGINT) {
    printf("\n");
  }
  if (main_handler && !main_handler->stopped()) {
    main_handler->stop();
  }
}

/***********************************************************************************/

io_handler luaos_main_ios() {
  return main_handler;
}

local_values::local_values() {
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

local_values::~local_values() {
  if (_L) {
    gL = nullptr;
    luaos_close(_L);
    _L = nullptr;
  }
}

local_values& local_values::instance() {
  static thread_local local_values _instance;
  gL = _instance._L;
  return _instance;
}

/***********************************************************************************/

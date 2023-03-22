

#include <os/os.h>
#include <lua_wrapper.h>
#include "3rd/ikcp.h"

typedef IUINT32 conv_type;
typedef IUINT32 time_type;
#define luakcp_metaname "luakcp-wrapper"

/***********************************************************************************/

class lua_kcp final {
  inline void set_rapid() {
    set_nodelay(1, 10, 2, 1);
  }
  inline void set_normal() {
    set_nodelay(0, 40, 0, 0);
  }
  inline void set_nodelay(int nd, int interval, int resend, int nc) {
    ikcp_nodelay(_kcp, nd, interval, resend, nc);
  }
  int        _ref;
  bool       _rapid;
  lua_State* _L;
  ikcpcb*    _kcp;
  time_type  _next;

public:
  inline lua_kcp(conv_type conv, bool is_rapid, bool is_stream)
    : _kcp(ikcp_create(conv, this))
    , _ref (0)
    , _next(0)
    , _L(nullptr)
    , _rapid(is_rapid)
  {
    ikcp_wndsize(_kcp, 128, 512);
    _kcp->rx_minrto = 50;
    _kcp->stream = is_stream ? 1 : 0;
    is_rapid ? set_rapid() : set_normal();
  }
  inline ~lua_kcp()
  {
    close();
  }
  inline bool is_open() const
  {
    return (_kcp != nullptr);
  }
  inline bool is_error() const
  {
    return (_kcp->state < 0);
  }
  inline void close()
  {
    if (_kcp) {
      ikcp_release(_kcp);
      _kcp = nullptr;
    }
    if (_L && _ref) {
      luaL_unref(_L, LUA_REGISTRYINDEX, _ref);
      _L   = nullptr;
      _ref = 0;
    }
  }
  inline void output(lua_State* L, int index)
  {
    _L   = L;
    _ref = luaL_ref(L, LUA_REGISTRYINDEX);
    ikcp_setoutput(_kcp, callback);
  }
  inline bool setmtu(int mtu)
  {
    return ikcp_setmtu(_kcp, mtu) == 0;
  }
  inline bool winsize(int sndsize, int rcvsize)
  {
    return ikcp_wndsize(_kcp, sndsize, rcvsize) == 0;
  }
  inline void update(time_type now)
  {
    if (is_error()) {
      return;
    }
    while (_L && now >= _next) {
      ikcp_update(_kcp, now);
      _next = ikcp_check(_kcp, now);
    }
  }
  inline bool send(const char* data, size_t size)
  {
    if (is_error()) {
      return false;
    }
    int ret = ikcp_send(_kcp, data, (int)size);
    if (ret == 0) {
      _next = 0;
      if (_rapid) ikcp_flush(_kcp);
    }
    return ret == 0;
  }
  inline bool input(const char* data, size_t size)
  {
    if (is_error()) {
      return false;
    }
    int ret = ikcp_input(_kcp, data, (long)size);
    if (ret == 0) {
      _next = 0;
    }
    return ret == 0;
  }
  inline size_t receive(char* buffer, size_t size)
  {
    if (is_error()) {
      return 0;
    }
    int recved = ikcp_recv(_kcp, buffer, (int)size);
    return (recved > 0) ? recved : 0;
  }

private:
  static int callback(const char *buf, int len, ikcpcb*, void *user)
  {
    auto kcp = (lua_kcp*)user;
    lua_State* L = kcp->_L;
    lua_rawget(L,  kcp->_ref);
    lua_pushlstring(L, buf, len);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
      lua_error(L);
    }
    return len;
  }
};

/***********************************************************************************/

static lua_kcp* luakcp_check(lua_State* L, int index = 1)
{
  return lexget_userdata<lua_kcp>(L, index, luakcp_metaname);
}

static int luakcp_setmtu(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pushboolean(L, kcp->setmtu((int)luaL_checkinteger(L, 2)) ? 1 : 0);
  return 1;
}

static int luakcp_setwinsize(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  int sndsize = (int)luaL_checkinteger(L, 2);
  int rcvsize = (int)luaL_checkinteger(L, 3);
  lua_pushboolean(L, kcp->winsize(sndsize, rcvsize) ? 1 : 0);
  return 1;
}

static int luakcp_error(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 1);
    return 1;
  }
  lua_pushboolean(L, kcp->is_error() ? 1 : 0);
  return 1;
}

static int luakcp_update(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  auto now = luaL_optinteger(L, 2, 0);
  kcp->update((time_type)(now ? now : os::milliseconds()));
  lua_pushboolean(L, 1);
  return 1;
}

static int luakcp_bind(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  luaL_argcheck(L, lua_type(L, 2) == LUA_TFUNCTION, 2, "function expected");
  kcp->output(L, 2);
  lua_pushboolean(L, 1);
  return 1;
}

static int luakcp_send(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  lua_pushboolean(L, kcp->send(data, size) ? 1 : 0);
  return 1;
}

static int luakcp_input(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  lua_pushboolean(L, kcp->input(data, size) ? 1 : 0);
  return 1;
}

static int luakcp_receive(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (!kcp->is_open()) {
    lua_pushnil(L);
    return 0;
  }
  char buffer[8192];
  size_t size = kcp->receive(buffer, sizeof(buffer));
  if (size == 0) {
    lua_pushnil(L);
  }
  else {
    lua_pushlstring(L, buffer, size);
  }
  return 1;
}

static int luakcp_gc(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  kcp->~lua_kcp();
  return 0;
}

static int luakcp_close(lua_State* L)
{
  lua_kcp* kcp = luakcp_check(L, 1);
  if (kcp->is_open()) kcp->close();
  return 0;
}

static int luakcp_create(lua_State* L)
{
  conv_type conv = (conv_type)luaL_checkinteger(L, 1);
  bool rapid  = luaL_optboolean(L, 2, false);
  bool stream = luaL_optboolean(L, 3, false);
  new (lexnew_userdata<lua_kcp>(L, luakcp_metaname)) lua_kcp(conv, rapid, stream);
  return 1;
}

static int luakcp_getconv(lua_State* L)
{
  auto conv = ikcp_getconv(luaL_checkstring(L, 1));
  lua_pushinteger(L, conv);
  return 1;
}

/***********************************************************************************/

extern "C" int luaopen_ikcp(lua_State * L)
{
  const luaL_Reg methods[] = {
    {"__gc",      luakcp_gc         },
    {"close",     luakcp_close      },
    {"bind",      luakcp_bind       },
    {"mtu",       luakcp_setmtu     },
    {"error",     luakcp_error      },
    {"winsize",   luakcp_setwinsize },
    {"update",    luakcp_update     },
    {"send",      luakcp_send       },
    {"input",     luakcp_input      },
    {"receive",   luakcp_receive    },
    {NULL,        NULL              }
  };
  lexnew_metatable(L, luakcp_metaname, methods);
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushcfunction(L, luakcp_create);
  lua_setfield(L, -2, "new");

  lua_pushcfunction(L, luakcp_getconv);
  lua_setfield(L, -2, "getconv");
  return 1;
}

/***********************************************************************************/

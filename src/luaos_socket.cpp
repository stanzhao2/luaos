
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
#include "luaos_socket.h"

/*******************************************************************************/

static void collectgarbage(lua_State* L)
{
  if (luaos_is_debug()) {
    lua_gc(L, LUA_GCCOLLECT, 0);
  }
}

lua_socket::lua_socket(socket::ref sock, family_type type)
  : _socket(sock), _type(type) {
}

lua_socket::~lua_socket()
{
  if (luaos_is_debug()) {
    luaos_print("%s\n", __FUNCTION__);
  }
}

reactor_type lua_socket::service() const
{
  return _socket->service();
}

void lua_socket::close()
{
  _socket->close(true);
}

bool lua_socket::is_open() const
{
  return _socket->is_open();
}

int lua_socket::id() const
{
  return _socket->id();
}

void lua_socket::context(int index)
{
  _socket->context((void*)(size_t)index);
}

void lua_socket::timeout(size_t milliseconds)
{
  _socket->timeout(milliseconds);
}

error_code lua_socket::connect(const char* host, unsigned short port, size_t timeout)
{
  return _socket->connect(host, port, timeout);
}

size_t lua_socket::receive(char* buf, size_t size, error_code& ec)
{
  return _socket->receive(buf, size, ec);
}

size_t lua_socket::receive_from(char* buf, size_t size, ip::udp::endpoint& peer, error_code& ec)
{
  return _socket->receive_from(buf, size, peer, ec);
}

void lua_socket::send(const char* data, size_t size)
{
  _socket->async_send(data, size);
}

size_t lua_socket::send(const char* data, size_t size, error_code& ec)
{
  return _socket->send(data, size, ec);
}

void lua_socket::send_to(const char* data, size_t size, const ip::udp::endpoint& peer)
{
  _socket->send_to(data, size, peer);
}

size_t lua_socket::send_to(const char* data, size_t size, const ip::udp::endpoint& peer, error_code& ec)
{
  return _socket->send_to(data, size, peer, ec);
}

/*******************************************************************************/

static void on_error(const error_code& ec, int index, socket_type peer)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  int sndref = (int)(size_t)peer->context();
  if (sndref > 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, sndref);
    peer->context(0);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    return;
  }
  luaL_unref(L, LUA_REGISTRYINDEX, index);
  if (peer->is_open()) {
    peer->close();
  }

  lua_pushinteger(L, ec.value());
  lua_pushstring (L, ec.message().c_str());
  if (luaos_pcall(L, 2, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  collectgarbage(L); //DEBUG: for windows only
}

static error_code on_read(size_t size, int index, socket_type peer)
{
  static thread_local std::string data;
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    return error::invalid_argument;
  }

  lua_pushinteger(L, 0); //no error
  lua_pushlstring(L, peer->receive(), size);
  if (luaos_pcall(L, 2, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    peer->close();
  }
  return error_code();
}

static void on_receive(const error_code& ec, size_t size, int index, socket_type peer)
{
  if (ec) {
    on_error(ec, index, peer);
    return;
  }
  error_code _ec = on_read(size, index, peer);
  if (_ec) {
    on_error(_ec, index, peer);
    return;
  }
  if (!peer->is_open()) {
    on_error(error::interrupted, index, peer);
  }
}

static error_code on_read_from(size_t size, int index, socket_type peer)
{
  static thread_local std::string data;
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  if (data.size() < size) {
    data.resize(size);
  }
  error_code ec;
  ip::udp::endpoint remote;
  size = peer->receive_from((char*)data.c_str(), size, remote, ec);
  if (ec || size == 0) {
    return ec;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    return error::invalid_argument;
  }

  lua_pushinteger(L, 0); //no error
  lua_pushlstring(L, data.c_str(), size);

  lua_newtable(L);  /* ip and port of table */
  lua_pushstring(L, remote.address().to_string().c_str());
  lua_setfield(L, -2, "ip");
  lua_pushinteger(L, remote.port());
  lua_setfield(L, -2, "port");

  if (luaos_pcall(L, 3, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    peer->close();
  }
  return ec;
}

static void on_receive_from(const error_code& ec, size_t size, int index, socket_type peer)
{
  if (ec) {
    on_error(ec, index, peer);
    return;
  }
  error_code _ec = on_read_from(size, index, peer);
  if (_ec) {
    on_error(_ec, index, peer);
    return;
  }
  if (!peer->is_open()) {
    on_error(error::interrupted, index, peer);
  }
}

static void on_send(const error_code& ec, size_t size, int index, socket_type peer)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  if (ec) {
    peer->close();
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    return;
  }

  lua_pushinteger(L, ec.value());
  if (luaos_pcall(L, 1, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    peer->close();
  }
}

static void on_accept(const error_code& ec, socket_type peer, int index, socket_type acceptor)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  if (ec) {
    if (!acceptor->is_open())
    {
      luaL_unref(L, LUA_REGISTRYINDEX, index);
      collectgarbage(L);
    }
    return;
  }

  lua_socket* lua_sock = new lua_socket(peer, family_type::tcp);
  if (!lua_sock) {
    return;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    delete lua_sock;
    return;
  }

  lua_socket** userdata = lexnew_userdata<lua_socket*>(L, lua_socket::metatable_name());
  if (!userdata) {
    delete lua_sock;
    return;
  }

  *userdata = lua_sock;
  if (luaos_pcall(L, 1, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    peer->close();
  }

  if (peer->is_open()) {
    if (peer->timeout() == 0) {
      peer->timeout(300 * 1000);
    }
  }
  else {
    collectgarbage(L); //DEBUG: for windows only
  }
}

static void on_connect(const error_code& ec, int index, socket_type peer)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  luaL_unref (L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    return;
  }

  lua_pushinteger(L, ec.value());
  if (luaos_pcall(L, 1, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    peer->close();
  }

  if (peer->is_open()) {
    if (peer->timeout() == 0) {
      peer->timeout(300 * 1000);
    }
  }
  else {
    collectgarbage(L); //DEBUG: for windows only
  }
}

static void on_handshake(const error_code& ec, int index, socket_type peer)
{
  lua_State* L = luaos_local.lua_state();
  stack_rollback rollback(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  luaL_unref (L, LUA_REGISTRYINDEX, index);
  if (!lua_isfunction(L, -1)) {
    return;
  }

  lua_pushinteger(L, ec.value());
  if (luaos_pcall(L, 1, 0) != LUA_OK) {
    luaos_error("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    peer->close();
  }

  if (!peer->is_open()) {
    collectgarbage(L); //DEBUG: for windows only
  }
}

static int lua_os_socket_bind(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  lua_socket* lua_sock = *mt;
  if (!lua_sock->is_udp()) {
    luaL_error(L, "socket must be udp protocol");
  }

  if (!lua_isfunction(L, 4)) {
    luaL_argerror(L, 4, "must be a function");
  }

  const char* host = luaL_checkstring(L, 2);
  unsigned short port = (unsigned short)luaL_checkinteger(L, 3);

  int handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  error_code ec = lua_sock->bind(
    port, host, std::bind(&on_receive_from, placeholders1, placeholders2, handler_ref, lua_sock->get_socket())
  );

  if (ec) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  ip::address addr;
  lua_sock->get_socket()->local_address(addr, &port);
  lua_pushboolean(L, 1);
  lua_pushinteger(L, port);
  return 2;
}

static int lua_os_socket_listen(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  lua_socket* lua_sock = *mt;
  if (!lua_sock->is_tcp()) {
    luaL_error(L, "socket must be tcp protocol");
  }

  const char* host = luaL_checkstring(L, 2);
  unsigned short port = (unsigned short)luaL_checkinteger(L, 3);
  if (!lua_isfunction(L, 4)) {
    luaL_argerror(L, 4, "must be a function");
  }

  int handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  error_code ec = lua_sock->listen(
    port, host, std::bind(&on_accept, placeholders1, placeholders2, handler_ref, lua_sock->get_socket())
  );

  if (ec) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  ip::address addr;
  lua_sock->get_socket()->local_address(addr, &port);
  lua_pushboolean(L, 1);
  lua_pushinteger(L, port);
  return 2;
}

static int lua_os_socket_connect(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  int handler_ref = 0, timeout = 5000;
  const char* host = luaL_checkstring(L, 2);
  unsigned short port = (unsigned short)luaL_checkinteger(L, 3);

  if (!lua_isnoneornil(L, 4))
  {
    int type = lua_type(L, 4);
    if (type == LUA_TFUNCTION) {
      handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (type == LUA_TNUMBER)
    {
      timeout = (int)luaL_checkinteger(L, 4);
      if (timeout < 1000) {
        timeout = 1000;
      }
    }
    else {
      luaL_argerror(L, 4, "is not integer or function");
    }
  }

  error_code ec;
  lua_socket* lua_sock = *mt;
  if (handler_ref == 0)
  {
    ec = lua_sock->connect(host, port, timeout);
    if (!ec) {
      if (lua_sock->timeout() == 0) {
        lua_sock->timeout(300 * 1000);
      }
    }
  }
  else
  {
    lua_sock->async_connect(
      host, port, std::bind(&on_connect, placeholders1, handler_ref, lua_sock->get_socket())
    );
  }

  if (ec) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int lua_os_socket_select(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  size_t type = luaL_checkinteger(L, 2);
  if (type != 1 && type != 2) {
    luaL_argerror(L, 2, "type error, must be 1 or 2");
  }
  if (!lua_isfunction(L, 3)) {
    luaL_argerror(L, 3, "must be a function");
  }

  lua_socket* lua_sock = *mt;
  int handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  auto raw_socket = lua_sock->get_socket();

  if (type == 1) {
    lua_sock->async_wait(
      socket::wait_type::wait_read,
      std::bind(&on_receive, placeholders1, placeholders2, handler_ref, raw_socket)
    );
  }
  else if (type == 2) {
    lua_sock->async_wait(
      socket::wait_type::wait_write,
      std::bind(&on_send, placeholders1, placeholders2, handler_ref, raw_socket)
    );
    lua_sock->context(handler_ref);
  }
  lua_pushboolean(L, 1);
  return 0;
}

static int lua_os_socket_endpoint(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  ip::address addr;
  unsigned short port = 0;
  lua_socket* lua_sock = *mt;

  lua_sock->get_socket()->remote_address(addr, &port);
  std::string ip = addr.to_string();

  lua_pushlstring(L, ip.c_str(), ip.size());
  lua_pushinteger(L, port);
  return 2;
}

static int lua_os_socket_encode(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  error_code ec;
  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  int opcode = (int)luaL_optinteger(L, 3, 1);
  if (opcode < 1 || opcode > 15) {
    luaL_argerror(L, 3, "opcode: [1 - 15]");
  }
  bool encrypt  = luaL_optboolean(L, 4, true);
  bool compress = luaL_optboolean(L, 5, true);

  std::string packet;
  lua_socket* lua_sock = *mt;
  lua_sock->encode(opcode, data, size, encrypt, compress, [&](const char* p, size_t n) {
    packet.append(p, n);
  });

  lua_pushlstring(L, packet.c_str(), packet.size());
  return 1;
}

static int lua_os_socket_send(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);

  error_code ec;
  lua_socket* lua_sock = *mt;
  if (luaL_optboolean(L, 3, false)) {
    lua_sock->send(data, size);
  }
  else {
    size = lua_sock->send(data, size, ec);
  }

  if (ec) {
    lua_pushnil(L);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushinteger(L, (lua_Integer)size);
  return 1;
}

static int lua_os_socket_send_to(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  lua_socket* lua_sock = *mt;
  if (!lua_sock->is_udp()) {
    luaL_error(L, "socket must be udp protocol");
  }

  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  const char* host = luaL_checkstring(L, 3);
  unsigned short port = (unsigned short)luaL_checkinteger(L, 4);
  bool async_send = luaL_optboolean(L, 5, false);

  error_code ec;
  auto remote = udp::resolve(host, port, ec);
  if (ec) {
    lua_pushnil(L);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }

  if (async_send) {
    lua_sock->send_to(data, size, *remote.begin());
  }
  else {
    size = lua_sock->send_to(data, size, *remote.begin(), ec);
  }

  if (ec) {
    lua_pushnil(L);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushinteger(L, (lua_Integer)size);
  return 1;
}

static int lua_os_socket_available(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }
  lua_socket* lua_sock = *mt;
  size_t available = lua_sock->get_socket()->available();
  lua_pushinteger(L, (lua_Integer)available);
  return 1;
}

static int lua_os_socket_nodelay(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  lua_socket* lua_sock = *mt;
  error_code ec = lua_sock->get_socket()->no_delay();
  if (ec) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int lua_os_socket_decode(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  if (!lua_isfunction(L, 3)) {
    luaL_argerror(L, 3, "must be a function");
  }

  int ec = 0;
  lua_socket* lua_sock = *mt;
  size = lua_sock->decode(data, size, ec, [L](const char* p, size_t n, const decoder::header* h)
  {
    lua_pushvalue(L, 3);
    lua_pushlstring(L, p, n);
    lua_pushinteger(L, h->opcode);
    if (luaos_pcall(L, 2, 0) != LUA_OK) {
      luaos_error("%s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  });

  if (ec > 0)
  {
    lua_pushnil(L);
    switch (ec) {
    case 1:
      lua_pushstring(L, "rsv3 is not zero");
      break;
    case 2:
      lua_pushstring(L, "hash check failed");
      break;
    default:
      lua_pushstring(L, "decompress failed");
      break;
    }
    return 2;
  }
  lua_pushinteger(L, (lua_Integer)size);
  return 1;
}

static int lua_os_socket_receive(lua_State* L)
{
  static thread_local std::string data;
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  error_code ec;
  lua_socket* lua_sock = *mt;
  size_t size = luaL_optinteger(L, 2, 8192);

  if (size > 8192) {
    size = 8192;
  }
  if (data.size() < size) {
    data.resize(size);
  }

  size = lua_sock->receive((char*)data.c_str(), size, ec);
  if (ec) {
    lua_pushnil(L);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushlstring(L, data.c_str(), size);
  return 1;
}

static int lua_os_socket_receive_from(lua_State* L)
{
  static thread_local std::string data;
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  lua_socket* lua_sock = *mt;
  if (!lua_sock->is_udp()) {
    luaL_error(L, "socket must be udp protocol");
  }

  error_code ec;
  ip::udp::endpoint remote;
  size_t size = luaL_optinteger(L, 2, 8192);

  if (size > 8192) {
    size = 8192;
  }
  if (data.size() < size) {
    data.resize(size);
  }

  size = lua_sock->receive_from((char*)data.c_str(), size, remote, ec);
  if (ec) {
    lua_pushnil(L);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushlstring(L, data.c_str(), size);
  lua_pushstring (L, remote.address().to_string().c_str());
  lua_pushinteger(L, remote.port());
  return 3;
}

static int lua_os_socket_timeout(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }

  lua_socket* lua_sock = *mt;
  if (!lua_sock->is_tcp()) {
    luaL_error(L, "socket must be tcp protocol");
  }

  lua_sock->timeout(luaL_checkinteger(L, 2));
  lua_pushboolean(L, 1);
  return 1;
}

static int lua_os_socket_id(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushnil(L);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }
  lua_socket* lua_sock = *mt;
  lua_pushinteger(L, lua_sock->id());
  return 1;
}

static int lua_os_socket_is_open(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "#1 not a socket object");
    return 2;
  }
  lua_socket* lua_sock = *mt;
  lua_pushboolean(L, lua_sock->is_open() ? 1 : 0);
  return 1;
}

static int lua_os_socket_gc(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (mt)
  {
    lua_socket* lua_sock = *mt;
    delete lua_sock;
    *mt = 0;
  }
  return 0;
}

static int lua_os_socket_close(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    return 0;
  }
  lua_socket* lua_sock = *mt;
  lua_pushboolean(L, lua_sock->is_open() ? 1 : 0);
  lua_sock->close();
  return 1;
}

static int lua_os_socket(lua_State* L)
{
  socket::ref sock;
  auto ios = luaos_local.lua_service();
  const char* family = luaL_optstring(L, -1, "tcp");

  lua_socket* lua_sock = 0;
  if (_tinydir_stricmp(family, "tcp") == 0)
  {
    sock = socket::create(
      ios, socket::family::sock_stream
    );
    lua_sock = new lua_socket(sock, family_type::tcp);
  }

  if (_tinydir_stricmp(family, "udp") == 0)
  {
    sock = socket::create(
      ios, socket::family::sock_dgram
    );
    lua_sock = new lua_socket(sock, family_type::udp);
  }

  if (!lua_sock) {
    return 0;
  }
  lua_socket** userdata = lexnew_userdata<lua_socket*>(L, lua_socket::metatable_name());
  if (userdata) {
    *userdata = lua_sock;
  }
  else {
    delete lua_sock;
  }
  return userdata ? 1 : 0;
}

/*******************************************************************************/

static bool ssl_read_file(const char* filename, std::string& data)
{
  data.clear();
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    return false;
  }
  char buffer[8192];
  while (!feof(fp)) {
    size_t n = fread(buffer, 1, sizeof(buffer), fp);
    data.append(buffer, n);
  }
  fclose(fp);
  return true;
}

static int ssl_sni_callback(SSL* ssl, int* al, void* arg)
{
#ifdef TLS_SSL_ENABLE
  int ref = (int)(size_t)arg;
  const char* servername = nullptr;
  servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

  if (ref > 0)
  {
    lua_State* L = luaos_local.lua_state();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (servername) {
      lua_pushstring(L, servername);
    } else {
      lua_pushnil(L);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    if (luaos_pcall(L, 1, 0) != LUA_OK) {
      luaos_error("%s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  return SSL_TLSEXT_ERR_OK;
#else
  return 0;
#endif
}

static int lua_os_socket_ssl_sni_callback(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  shared_ctx** userdata = lua_socket::check_ssl_metatable(L);
  if (!userdata) {
    return 0;
  }
  luaL_argcheck(L, lua_isfunction(L, 2), 2, "must be function");
  auto ctx = (*userdata)->ctx;
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);

  auto ctx_handle = ctx->native_handle();
  SSL_CTX_set_tlsext_servername_callback(ctx_handle, ssl_sni_callback);
  SSL_CTX_set_tlsext_servername_arg(ctx_handle, (void*)(size_t)ref);
  lua_pushboolean(L, 1);
#else
  lua_pushboolean(L, 0);
#endif
  return 1;
}

static int lua_os_socket_ssl_load_ca(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  shared_ctx** userdata = lua_socket::check_ssl_metatable(L);
  if (!userdata) {
    return 0;
  }
  error_code ec;
  const char* certfile = luaL_checkstring(L, 2);
  auto ctx = (*(userdata))->ctx;
  ctx->load_verify_file(certfile, ec);
  if (ec) {
    lua_pushboolean(L, 0);
    return 1;
  }
  else {
    ctx->set_verify_mode(ssl::verify_peer);
    ctx->set_verify_mode(ssl::verify_fail_if_no_peer_cert);
  }
  lua_pushboolean(L, 1);
#else
  lua_pushboolean(L, 0);
#endif
  return 1;
}

static int lua_os_socket_ssl_assign(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  shared_ctx** userdata = lua_socket::check_ssl_metatable(L);
  if (!userdata) {
    return 0;
  }
  auto ctx = (*userdata)->ctx;
  size_t certsize = 0, keysize = 0;
  const char* certfile = luaL_checklstring(L, 2, &certsize);
  const char* key      = luaL_checklstring(L, 3, &keysize);
  const char* pwd      = luaL_optstring(L, 4, nullptr);

  error_code ec;
  ctx->use_certificate_chain(buffer(certfile, certsize), ec);
  if (ec) {
    lua_pushboolean(L, 0);
    return 1;
  }
  ctx->use_private_key(buffer(key, keysize), ssl::context::pem, ec);
  if (ec) {
    lua_pushboolean(L, 0);
    return 1;
  }
  if (pwd) {
    std::string cpwd(pwd);
    ctx->set_password_callback(std::bind([cpwd](size_t, ssl::context::password_purpose) {
      return cpwd.c_str();
    }, placeholders1, placeholders2), ec);
    if (ec) {
      lua_pushboolean(L, 0);
      return 1;
    }
  }
  lua_pushboolean(L, 1);
#else
  lua_pushboolean(L, 0);
#endif
  return 1;
}

static int lua_os_socket_ssl_load(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  shared_ctx** userdata = lua_socket::check_ssl_metatable(L);
  if (!userdata) {
    return 0;
  }
  std::string certdata;
  if (!ssl_read_file(luaL_checkstring(L, 2), certdata)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  std::string keydata;
  if (!ssl_read_file(luaL_checkstring(L, 3), keydata)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  const char* password = luaL_optstring(L, 4, nullptr);
  std::string keypwd(password ? password : "");
  lua_pop(L, lua_gettop(L) - 1);

  lua_pushlstring(L, certdata.c_str(), certdata.size());
  lua_pushlstring(L, keydata.c_str(), keydata.size());
  if (password) {
    lua_pushlstring(L, keypwd.c_str(), keypwd.size());
  }
  return lua_os_socket_ssl_assign(L);
#else
  return 0;
#endif
}

static int lua_os_socket_ssl_context(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  shared_ctx* shared = new shared_ctx(ssl::context_base::tlsv12);
  if (!shared) {
    return 0;
  }
  auto ctx = shared->ctx;
  ctx->set_options(ssl::context::default_workarounds
    | ssl::context::no_sslv2
    | ssl::context::no_sslv3
    | ssl::context::no_tlsv1
    | ssl::context::single_dh_use
  );
  error_code ec;
  ctx->set_default_verify_paths(ec);
  shared_ctx** userdata = lexnew_userdata<shared_ctx*>(L, lua_socket::ssl_metatable_name());
  if (userdata) {
    *userdata = shared;
  } else {
    delete shared;
  }
  return userdata ? 1 : 0;
#else
  return 0;
#endif
}

static int lua_os_socket_ssl_gc(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  shared_ctx** userdata = lua_socket::check_ssl_metatable(L);
  if (!userdata) {
    return 0;
  }
  delete *userdata;
  *userdata = nullptr;
#endif
  return 0;
}

static int lua_os_socket_ssl_close(lua_State* L)
{
  return lua_os_socket_ssl_gc(L);
}

static int lua_os_socket_ssl_sni(lua_State* L)
{
#ifdef TLS_SSL_ENABLE
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    return 0;
  }
  lua_socket* lua_sock = *mt;
  if (lua_isstring(L, 2)) { /* server name */
    SSL* ssl_handle = lua_sock->ssl_handle();
    if (ssl_handle) {
      SSL_set_tlsext_host_name(ssl_handle, luaL_checkstring(L, 2));
      lua_pushboolean(L, 1);
      return 1;
    }
  }
#endif
  lua_pushboolean(L, 0);
  return 1;
}

static int lua_os_socket_ssl_enable(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    return 0;
  }
#ifdef TLS_SSL_ENABLE
  shared_ctx** userdata = lua_socket::check_ssl_metatable(L, 2);
  if (!userdata) {
    return 0;
  }
  lua_socket* lua_sock = *mt;
  lua_sock->ssl_enable((*userdata)->ctx);
  lua_pushboolean(L, 1);
#else
  lua_pushboolean(L, 0);
#endif
  return 1;
}

static int lua_os_socket_ssl_handshake(lua_State* L)
{
  lua_socket** mt = lua_socket::check_metatable(L);
  if (!mt) {
    return 0;
  }
  int handler_ref = 0;
  if (!lua_isnoneornil(L, 2))
  {
    if (lua_isfunction(L, 2)) {
      handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else {
      luaL_argerror(L, 2, "must be a function");
    }
  }
  error_code ec;
  lua_socket* lua_sock = *mt;
  if (handler_ref == 0) {
    ec = lua_sock->handshake();
  }
  else {
    lua_sock->handshake(
      std::bind(&on_handshake, placeholders1, handler_ref, lua_sock->get_socket())
    );
  }
  if (ec) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, ec.message().c_str());
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

/*******************************************************************************/

const char* lua_socket::metatable_name()
{
  return "luaos-socket";
}

void lua_socket::init_metatable(lua_State* L)
{
  struct luaL_Reg __call[] = {
    { "__call",       lua_os_socket },
    { NULL,           NULL          },
  };
  lua_getglobal(L, "io"); /* push os to stack */
  lua_newtable(L);        /* push table to stack */

  lua_pushinteger(L, 1);
  lua_setfield(L, -2, "read");

  lua_pushinteger(L, 2);
  lua_setfield(L, -2, "write");

  lexnew_metatable(L, "io.socket", __call);
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "socket");
  lua_pop(L, 1); //pop io from stack

  struct luaL_Reg methods[] = {
    { "__gc",         lua_os_socket_gc            },
    { "listen",       lua_os_socket_listen        },
    { "bind",         lua_os_socket_bind          },
    { "connect",      lua_os_socket_connect       },
    { "close",        lua_os_socket_close         },
    { "is_open",      lua_os_socket_is_open       },
    { "id",           lua_os_socket_id            },
    { "nodelay",      lua_os_socket_nodelay       },
    { "available",    lua_os_socket_available     },
    { "timeout",      lua_os_socket_timeout       },
    { "endpoint",     lua_os_socket_endpoint      },
    { "select",       lua_os_socket_select        },
    { "encode",       lua_os_socket_encode        },
    { "send",         lua_os_socket_send          },
    { "send_to",      lua_os_socket_send_to       },
    { "decode",       lua_os_socket_decode        },
    { "receive",      lua_os_socket_receive       },
    { "receive_from", lua_os_socket_receive_from  },
    { "tls_sni",      lua_os_socket_ssl_sni       },
    { "tls_enable",   lua_os_socket_ssl_enable    },
    { "handshake",    lua_os_socket_ssl_handshake },
    { NULL,           NULL                        },
  };
  lexnew_metatable(L, metatable_name(), methods);
  lua_pop(L, 1);
}

lua_socket** lua_socket::check_metatable(lua_State* L)
{
  lua_socket** self = lexget_userdata<lua_socket*>(L, 1, metatable_name());
  if (!self || !(*self)) {
    return nullptr;
  }
  return self;
}

/*******************************************************************************/

const char* lua_socket::ssl_metatable_name()
{
  return "luaos-ssl-context";
}

void lua_socket::init_ssl_metatable(lua_State* L)
{
  lua_newtable(L);
  lua_pushcfunction(L, lua_os_socket_ssl_context);
  lua_setfield(L, -2, "context");
  lua_setglobal(L, "tls");

  struct luaL_Reg methods[] = {
    { "__gc",         lua_os_socket_ssl_gc            },
    { "assign",       lua_os_socket_ssl_assign        },
    { "ca",           lua_os_socket_ssl_load_ca       },
    { "load",         lua_os_socket_ssl_load          },
    { "sni_callback", lua_os_socket_ssl_sni_callback  },
    { "close",        lua_os_socket_ssl_close         },
    { NULL,           NULL },
  };
  lexnew_metatable(L, ssl_metatable_name(), methods);
  lua_pop(L, 1);
}

#ifdef TLS_SSL_ENABLE
shared_ctx** lua_socket::check_ssl_metatable(lua_State* L, int index)
{
  shared_ctx** self = lexget_userdata<shared_ctx*>(L, index, ssl_metatable_name());
  if (!self || !(*self)) {
    return nullptr;
  }
  return self;
}
#endif

/*******************************************************************************/

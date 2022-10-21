
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

#include "luaos_thread_local.h"

/*******************************************************************************/

enum struct family_type {
  tcp = 0,
  udp = 1,
  acceptor = 2
};

class lua_socket final {
  family_type      _type;
  socket_type      _socket;

public:
  lua_socket(socket::ref, family_type);
  virtual ~lua_socket();
  void close();
  void timeout(size_t milliseconds);
  void context(int index);
  int  id() const;
  bool is_open() const;

  reactor_type service() const;
  error_code connect(const char* host, unsigned short port, size_t timeout);

public:
  size_t receive(char* buf, size_t size, error_code& ec);
  size_t receive_from(char* buf, size_t size, ip::udp::endpoint& peer, error_code& ec);
  size_t send(const char* data, size_t size, error_code& ec);
  size_t send_to(const char* data, size_t size, const ip::udp::endpoint& peer, error_code& ec);
  void   send(const char* data, size_t size);
  void   send_to(const char* data, size_t size, const ip::udp::endpoint& peer);

public:
  inline socket_type get_socket() const {
    return _socket;
  }
  inline size_t timeout() const {
    return _socket->timeout();
  }
  inline bool is_tcp() const {
    return _type == family_type::tcp;
  }
  inline bool is_udp() const {
    return _type == family_type::udp;
  }
  inline bool is_acceptor() const {
    return _type == family_type::acceptor;
  }
  template <typename Handler>
  size_t decode(const char* data, size_t size, int& ec, Handler handler)
  {
    return _socket->decode(data, size, ec, handler);
  }
  template <typename Handler>
  void encode(int opcode, const char* data, size_t size, bool encrypt, bool compress, Handler handler)
  {
    _socket->encode(opcode, data, size, encrypt, compress, handler);
  }
  template <typename Handler>
  int bind(unsigned short port, const char* host, Handler handler)
  {
    auto ec = _socket->bind(port, host, handler);
    return ec.value();
  }
  template <typename Handler>
  int listen(unsigned short port, const char* host, Handler handler)
  {
    _type = family_type::acceptor;
    auto ec = _socket->listen(port, host, 16, handler);
    return ec.value();
  }
  template <typename Handler>
  void async_connect(const char* host, unsigned short port, Handler handler) {
    _socket->async_connect(host, port, handler);
  }
  template <typename Handler>
  void async_wait(socket::wait_type type, Handler handler) {
    _socket->async_wait(type, handler);
  }

public:
  static const char* metatable_name();
  static void init_metatable(lua_State* L);
  static lua_socket** check_metatable(lua_State* L);
};

/*******************************************************************************/

LUALIB_API int lua_os_socket             (lua_State* L);
LUALIB_API int lua_os_socket_close       (lua_State* L);
LUALIB_API int lua_os_socket_gc          (lua_State* L);
LUALIB_API int lua_os_socket_id          (lua_State* L);
LUALIB_API int lua_os_socket_is_open     (lua_State* L);
LUALIB_API int lua_os_socket_timeout     (lua_State* L);
LUALIB_API int lua_os_socket_bind        (lua_State* L);
LUALIB_API int lua_os_socket_listen      (lua_State* L);
LUALIB_API int lua_os_socket_connect     (lua_State* L);
LUALIB_API int lua_os_socket_endpoint    (lua_State* L);
LUALIB_API int lua_os_socket_nodelay     (lua_State* L);
LUALIB_API int lua_os_socket_select      (lua_State* L);
LUALIB_API int lua_os_socket_encode      (lua_State* L);
LUALIB_API int lua_os_socket_send        (lua_State* L);
LUALIB_API int lua_os_socket_send_to     (lua_State* L);
LUALIB_API int lua_os_socket_available   (lua_State* L);
LUALIB_API int lua_os_socket_decode      (lua_State* L);
LUALIB_API int lua_os_socket_receive     (lua_State* L);
LUALIB_API int lua_os_socket_receive_from(lua_State* L);

/*******************************************************************************/

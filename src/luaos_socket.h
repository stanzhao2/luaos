
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

#include "luaos_io.h"

using namespace eth;
typedef socket::ref socket_type;

/*******************************************************************************/

enum struct family_type {
  tcp = 0,
  udp = 1,
  acceptor = 2
};

#ifdef TLS_SSL_ENABLE
struct shared_ctx {
  shared_ctx(ssl::context_base::method method)
    : ctx(new tls::ssl_context(method)) {
  }
  std::shared_ptr<tls::ssl_context> ctx;
};
#endif

class lua_socket final {
  family_type      _type;
  socket_type      _socket;
#ifdef TLS_SSL_ENABLE
  std::shared_ptr<tls::ssl_context> _ctx;
#endif

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
#ifdef TLS_SSL_ENABLE
  inline void ssl_enable(std::shared_ptr<tls::ssl_context> ctx) {
    _ctx = ctx;
    _socket->ssl_enable(*ctx);
  }
#endif
  inline error_code handshake() {
    error_code ec;
    _socket->handshake(ec);
    return ec;
  }
  template <typename Handler>
  void handshake(Handler handler) {
    _socket->async_handshake(handler);
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
  error_code bind(unsigned short port, const char* host, Handler handler)
  {
    return _socket->bind(port, host, handler);
  }
  template <typename Handler>
  error_code listen(unsigned short port, const char* host, Handler handler)
  {
    _type = family_type::acceptor;
    return _socket->listen(port, host, 16, handler);
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

  static const char* ssl_metatable_name();
  static void init_ssl_metatable(lua_State* L);

#ifdef TLS_SSL_ENABLE
  static shared_ctx** check_ssl_metatable(lua_State* L, int index = 1);
#endif
};

/*******************************************************************************/

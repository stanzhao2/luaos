

#pragma once

/********************************************************************************/

#include "ios_ssl.h"

/********************************************************************************/

/********************************************************************************/
namespace asio {
/********************************************************************************/
namespace ip {
/********************************************************************************/

enum struct family {
  sock_stream, sock_dgram
};

/********************************************************************************/

class socket final : public udp::socket
  , public std::enable_shared_from_this<socket> {
  typedef udp::socket parent;
  typedef ip::udp::resolver::results_type results_type;
  typedef openssl::callback_t callback_t;

  socket(io::service::value ios, family type)
    : _ios(ios)
    , _family(type)
    , parent(*ios) {
  }

  socket(io::service::value ios, openssl::context::value ctx, family type)
    : _ios(ios)
    , _family(type)
    , _context(ctx)
    , parent(*ios) {
  }

  void init_socket(openssl::context::value ctx) {
    if (_socket)
      return;
    _socket = ctx ? openssl::socket::create(_ios, ctx) : openssl::socket::create(_ios);
  }

  void init_acceptor() {
    if (_acceptor)
      return;
    _acceptor = openssl::acceptor::create(_ios);
  }

  void on_callback(const error_code& ec, size_t bytes) {
    if (_sent) _sent(ec, bytes);
  }

  void on_wait(const error_code& ec, const callback_t& handler) {
    size_t bytes = available();
    handler(ec, bytes > DEF_RBUFF_SIZE ? DEF_RBUFF_SIZE : bytes);
  }

  void on_select(const error_code& ec, size_t bytes, const callback_t& handler) {
    handler(ec, bytes);
    if (!ec) {
      async_select(wait_type::wait_read, handler);
    }
  }

private:
  socket(const socket&) = delete;
  family                    _family;
  callback_t                _sent;
  io::identifier            _id;
  openssl::context::value   _context;
  io::service::value        _ios;
  openssl::socket::value    _socket;
  openssl::acceptor::value  _acceptor;

public:
  typedef std::shared_ptr<socket> value;
  typedef openssl::socket::wait_type wait_type;
  typedef openssl::context context;

  static value create(io::service::value ios) {
    return value(new socket(ios, family::sock_stream));
  }

  static value create(io::service::value ios, family type) {
    return value(new socket(ios, type));
  }

  static value create(io::service::value ios, context::value ctx) {
    return value(new socket(ios, ctx, family::sock_stream));
  }

  static value create(io::service::value ios, context::value ctx, family type) {
    return value(new socket(ios, ctx, type));
  }

  inline io::service::value service() const {
    return _ios;
  }

  inline int id() const {
    return (int)_id.value();
  }

  inline family type() const {
    return _family;
  }

  inline bool is_udp_socket() const {
    return type() == family::sock_dgram;
  }

  inline bool is_tcp_socket() const {
    return type() == family::sock_stream;
  }

  inline context::value ssl_context() const {
    return _context;
  }

  void close(bool linger = false)
  {
    if (is_udp_socket()) {
      error_code ec;
      parent::shutdown(shutdown_type::shutdown_both);
      parent::close(ec);
      return;
    }
    if (_acceptor) {
      _acceptor->close();
    }
    if (_socket) {
      linger ? _socket->async_close() : _socket->close();
    }
  }

  void cancel()
  {
    error_code ec;
    if (is_udp_socket()) {
      parent::cancel(ec);
      return;
    }
    if (_socket) {
      _socket->cancel(ec);
      return;
    }
    if (_acceptor) {
      _acceptor->cancel(ec);
    }
  }

  bool is_open() const
  {
    if (is_udp_socket()) {
      return parent::is_open();
    }
    if (_socket) {
      return _socket->is_open();
    }
    if (_acceptor) {
      return _acceptor->is_open();
    }
    return false;
  }

  void assign(context::value ctx)
  {
    assert(ctx);
    if (is_udp_socket()) {
      return;
    }
    if (_acceptor) {
      return;
    }
    if (!_socket) {
      init_socket(ctx);
      return;
    }
    _context = ctx;
    _socket->assign(ctx);
  }

  error_code bind(unsigned short port, const char* host = nullptr)
  {
    assert(!_context);
    if (is_tcp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
    auto local = resolve(host, port, ec);
    if (ec) {
      return ec;
    }
    auto begin = *local.begin();
    auto endpoint = begin.endpoint();
    open(endpoint.protocol(), ec);
    if (ec) {
      return ec;
    }
#ifndef _MSC_VER
    reuse_address option(true);
    set_option(option, ec);
    if (ec) {
      return ec;
    }
#endif
    parent::bind(endpoint, ec);
    return ec;
  }

  error_code listen(unsigned short port, const char* host = nullptr, int backlog = 16)
  {
    assert(!_context);
    if (is_udp_socket()) {
      return error::operation_not_supported;
    }
    if (_socket) {
      return error::operation_not_supported;
    }
    init_acceptor();
    return _acceptor->listen(port, host, backlog);
  }

  error_code accept(socket::value peer)
  {
    assert(peer);
    if (is_udp_socket()) {
      return error::operation_not_supported;
    }
    if (!_acceptor) {
      return error::bad_descriptor;
    }
    peer->init_socket(_context);
    return _acceptor->accept(peer->_socket);
  }

  error_code connect(const char* host, unsigned short port)
  {
    assert(host && port);
    if (is_udp_socket()) {
      error_code ec;
      auto remote = resolve(host, port, ec);
      if (!ec) {
        parent::connect(*remote.begin(), ec);
      }
      return ec;
    }
    if (_acceptor) {
      return error::operation_not_supported;
    }
    init_socket(_context);
    return _socket->connect(host, port);
  }

  error_code wait(wait_type what)
  {
    if (is_udp_socket()) {
      error_code ec;
      parent::wait(what, ec);
      return ec;
    }
    if (_socket) {
      return _socket->wait(what);
    }
    return error::not_connected;
  }

  size_t available() const
  {
    error_code ec;
    return available(ec);
  }

  size_t available(error_code& ec) const
  {
    if (_socket) {
      return _socket->available(ec);
    }
    if (_acceptor) {
      ec = error::operation_not_supported;
      return 0;
    }
    return parent::available(ec);
  }

  size_t receive(char* data, size_t size)
  {
    error_code ec;
    return receive(data, size, ec);
  }

  size_t receive(char* data, size_t size, error_code& ec)
  {
    assert(data);
    if (is_udp_socket()) {
      return parent::receive(buffer(data, size), 0, ec);
    }
    if (_socket) {
      return _socket->read_some(buffer(data, size), ec);
    }
    ec = error::not_connected;
    return 0;
  }

  size_t receive_from(char* data, size_t size, udp::endpoint& remote)
  {
    error_code ec;
    return receive_from(data, size, remote, ec);
  }

  size_t receive_from(char* data, size_t size, udp::endpoint& remote, error_code& ec)
  {
    assert(data);
    if (is_tcp_socket()) {
      ec = error::operation_not_supported;
      return 0;
    }
    return parent::receive_from(buffer(data, size), remote, 0, ec);
  }

  size_t send(const char* data, size_t size)
  {
    error_code ec;
    return send(data, size, ec);
  }

  size_t send(const char* data, size_t size, error_code& ec)
  {
    assert(data);
    if (is_udp_socket()) {
      return parent::send(buffer(data, size), 0, ec);
    }
    if (_socket) {
      return write(*_socket, buffer(data, size), ec);
    }
    ec = error::not_connected;
    return 0;
  }

  size_t send_to(const char* data, size_t size, const udp::endpoint& remote)
  {
    error_code ec;
    return send_to(data, size, remote, ec);
  }

  size_t send_to(const char* data, size_t size, const udp::endpoint& remote, error_code& ec)
  {
    assert(data);
    if (is_tcp_socket()) {
      ec = error::operation_not_supported;
      return 0;
    }
    return parent::send_to(buffer(data, size), remote, 0, ec);
  }

public:
  static results_type resolve(
    const char* host, unsigned short port, error_code& ec) {
    char service[8];
    sprintf(service, "%u", port);
    if (!host || !host[0]) {
      host = "::";
    }
    io_context ios;
    ip::udp::resolver resolver(ios);
    return resolver.resolve(host, service, ec);
  }

  error_code local_endpoint(udp::endpoint& local)
  {
    error_code ec(
      error::operation_not_supported
    );
    if (is_udp_socket()) {
      local = parent::local_endpoint(ec);
    }
    return ec;
  }

  error_code local_endpoint(tcp::endpoint& local)
  {
    error_code ec(
      error::operation_not_supported
    );
    if (is_udp_socket()) {
      return ec;
    }
    if (_socket) {
      local = _socket->local_endpoint(ec);
      return ec;
    }
    if (_acceptor) {
      local = _acceptor->local_endpoint(ec);
      return ec;
    }
    return error::not_connected;
  }

  error_code remote_endpoint(udp::endpoint& local)
  {
    error_code ec(
      error::operation_not_supported
    );
    if (is_udp_socket()) {
      local = parent::remote_endpoint(ec);
    }
    return ec;
  }

  error_code remote_endpoint(tcp::endpoint& local)
  {
    error_code ec(
      error::operation_not_supported
    );
    if (is_udp_socket()) {
      return ec;
    }
    if (_socket) {
      local = _socket->remote_endpoint(ec);
      return ec;
    }
    return error::not_connected;
  }

public:
  template <typename Handler>
  void async_select(wait_type what, Handler handler)
  {
    assert(what != wait_type::wait_error);
    if (what == wait_type::wait_write) {
      _sent = handler;
      return;
    }
    async_wait(what,
      std::bind(
        &socket::on_select, shared_from_this(), std::placeholders::_1, std::placeholders::_2, (callback_t)handler
      )
    );
  }

  template <typename Handler>
  void async_accept(socket::value peer, Handler&& handler)
  {
    assert(peer);
    if (is_udp_socket()) {
      return;
    }
    if (_socket) {
      service()->post([handler]() {
        handler(error::operation_not_supported);
      });
      return;
    }
    peer->init_socket(_context);
    _acceptor->async_accept(peer->_socket, handler);
  }

  template <typename Handler>
  void async_connect(const char* host, unsigned short port, Handler&& handler)
  {
    assert(host);
    if (is_udp_socket())
    {
      error_code ec;;
      auto remote = resolve(host, port, ec);
      if (!ec) {
        parent::async_connect(*remote.begin(), handler);
      }
      return;
    }
    if (_acceptor) {
      service()->post([handler]() {
        handler(error::operation_not_supported);
      });
      return;
    }
    init_socket(_context);
    _socket->async_connect(host, port, handler);
  }

  template <typename Handler>
  void async_wait(wait_type what, Handler&& handler)
  {
    if (is_udp_socket()) {
      parent::async_wait(what,
        std::bind(
          &socket::on_wait, shared_from_this(), std::placeholders::_1, (callback_t)handler
        )
      );
      return;
    }
    if (_socket) {
      _socket->async_wait(what, handler);
      return;
    }
    service()->post([handler]() {
      handler(error::not_connected, 0);
    });
  }

  template <typename Handler>
  void async_receive(char* data, size_t size, Handler&& handler)
  {
    assert(data);
    if (is_udp_socket()) {
      parent::async_receive(buffer(data, size), handler);
      return;
    }
    if (_socket) {
      _socket->async_read_some(buffer(data, size), handler);
      return;
    }
    service()->post([handler]() {
      handler(error::not_connected, 0);
    });
  }

  template <typename Handler>
  void async_receive_from(char* data, size_t size, udp::endpoint& remote, Handler&& handler)
  {
    assert(data);
    if (is_udp_socket()) {
      parent::async_receive_from(buffer(data, size), remote, handler);
      return;
    }
    service()->post([handler]() {
      handler(error::operation_not_supported, 0);
    });
  }

  void async_send(const char* data, size_t size)
  {
    async_send(data, size,
      std::bind(
        &socket::on_callback, shared_from_this(),
        std::placeholders::_1,
        std::placeholders::_2
      )
    );
  }

  template <typename Handler>
  void async_send(const char* data, size_t size, Handler&& handler)
  {
    assert(data);
    if (is_udp_socket()) {
      parent::async_send(buffer(data, size), handler);
      return;
    }
    if (_socket) {
      _socket->async_send(data, size, handler);
      return;
    }
    service()->post([handler]() {
      handler(error::not_connected, 0);
    });
  }

  void async_send_to(const char* data, size_t size, const udp::endpoint& remote)
  {
    async_send_to(data, size, remote,
      std::bind(
        &socket::on_callback, shared_from_this(),
        std::placeholders::_1,
        std::placeholders::_2
      )
    );
  }

  template <typename Handler>
  void async_send_to(const char* data, size_t size, const udp::endpoint& remote, Handler&& handler)
  {
    assert(data);
    if (is_udp_socket()) {
      parent::async_send_to(buffer(data, size), remote, handler);
      return;
    }
    service()->post([handler]() {
      handler(error::operation_not_supported, 0);
    });
  }
};

/********************************************************************************/
} //end of namespace ip
/********************************************************************************/
} //end of namespace asio
/********************************************************************************/

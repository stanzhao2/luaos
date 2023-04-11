

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

class socket final : public udp::socket {
  typedef udp::socket parent;
  typedef ip::udp::resolver::results_type results_type;

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

  void init_socket(openssl::context::value ctx = nullptr) {
    if (_socket)
      return;
    _socket = ctx ? openssl::socket::create(_ios, ctx) : openssl::socket::create(_ios);
  }

  void init_acceptor() {
    if (_acceptor)
      return;
    _acceptor = openssl::acceptor::create(_ios);
  }

  bool is_udp_socket() const {
    return type() == family::sock_dgram;
  }

  bool is_tcp_socket() const {
    return type() == family::sock_stream;
  }

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

private:
  socket(const socket&) = delete;
  family                    _family;
  io::identifier            _id;
  openssl::context::value   _context;
  io::service::value        _ios;
  openssl::socket::value    _socket;
  openssl::acceptor::value  _acceptor;

public:
  typedef struct {
    std::string addr;
    unsigned short port;
  } address_t;

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

  inline context::value ssl_context() const {
    return _context;
  }

  void close(bool linger = false)
  {
    if (is_udp_socket())
    {
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

  error_code connect(const char* host, unsigned short port)
  {
    assert(host && port);
    if (is_udp_socket())
    {
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

  error_code local_endpoint(tcp::endpoint& local)
  {
    if (is_udp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
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

  error_code local_endpoint(udp::endpoint& local)
  {
    if (is_tcp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
    local = parent::local_endpoint(ec);
    return ec;
  }

  error_code local_endpoint(address_t& local)
  {
    error_code ec;
    if (is_udp_socket())
    {
      udp::endpoint end;
      end = parent::local_endpoint(ec);
      if (!ec) {
        local.addr = end.address().to_string();
        local.port = end.port();
      }
      return ec;
    }
    tcp::endpoint end;
    ec = local_endpoint(end);
    if (!ec) {
      local.addr = end.address().to_string();
      local.port = end.port();
    }
    return ec;
  }

  error_code remote_endpoint(tcp::endpoint& local)
  {
    if (is_udp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
    if (_socket) {
      local = _socket->remote_endpoint(ec);
      return ec;
    }
    return error::not_connected;
  }

  error_code remote_endpoint(udp::endpoint& local)
  {
    if (is_tcp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
    local = parent::remote_endpoint(ec);
    return ec;
  }

  error_code remote_endpoint(address_t& remote)
  {
    error_code ec;
    if (is_udp_socket())
    {
      udp::endpoint end;
      ec = remote_endpoint(end);
      if (!ec) {
        remote.addr = end.address().to_string();
        remote.port = end.port();
      }
      return ec;
    }
    tcp::endpoint end;
    ec = remote_endpoint(end);
    if (!ec) {
      remote.addr = end.address().to_string();
      remote.port = end.port();
    }
    return ec;
  }

  error_code available(size_t* size) const
  {
    assert(size);
    if (_acceptor) {
      return error::operation_not_supported;
    }
    error_code ec;
    size_t n = 0;
    if (_socket) {
      n = _socket->available(ec);
    }
    else {
      n = parent::available(ec);
    }
    if (size) *size = n;
    return ec;
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

  error_code wait(wait_type what)
  {
    if (is_udp_socket())
    {
      error_code ec;
      parent::wait(what, ec);
      return ec;
    }
    if (_socket) {
      return _socket->wait(what);
    }
    return error::not_connected;
  }

  error_code receive(char* data, size_t size, size_t* bytes)
  {
    assert(data && bytes);
    error_code ec;
    size_t n = 0;
    if (is_udp_socket()) {
      n = parent::receive(buffer(data, size), 0, ec);
    }
    else if (_socket) {
      n = _socket->read_some(buffer(data, size), ec);
    }
    else {
      ec = error::not_connected;
    }
    if (bytes) *bytes = n;
    return ec;
  }

  error_code receive_from(char* data, size_t size, address_t& remote, size_t* bytes)
  {
    assert(data && bytes);
    udp::endpoint peer;
    error_code ec = receive_from(data, size, peer, bytes);
    if (!ec) {
      remote.addr = peer.address().to_string();
      remote.port = peer.port();
    }
    return ec;
  }

  error_code receive_from(char* data, size_t size, udp::endpoint& remote, size_t* bytes)
  {
    assert(data && bytes);
    if (is_tcp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
    size_t n = parent::receive_from(buffer(data, size), remote, 0, ec);
    if (bytes) *bytes = n;
    return ec;
  }

  error_code send(const char* data, size_t size, size_t* bytes = nullptr)
  {
    assert(data);
    error_code ec;
    size_t n = 0;
    if (is_udp_socket()) {
      n = parent::send(buffer(data, size), 0, ec);
    }
    else if (_socket) {
      n = write(*_socket, buffer(data, size), ec);
    }
    else {
      ec = error::not_connected;
    }
    if (bytes) *bytes = n;
    return ec;
  }

  void async_send(const char* data, size_t size)
  {
    assert(data);
    async_send(data, size, [](const error_code&, size_t) {});
  }

  error_code send_to(const char* data, size_t size, const address_t& remote, size_t* bytes = nullptr)
  {
    assert(data);
    error_code ec;
    auto peer = resolve(remote.addr.c_str(), remote.port, ec);
    if (ec) {
      return ec;
    }
    return send_to(data, size, *peer.begin(), bytes);
  }

  void async_send_to(const char* data, size_t size, const address_t& remote)
  {
    assert(data);
    async_send_to(data, size, remote, [](const error_code&, size_t) {});
  }

  error_code send_to(const char* data, size_t size, const udp::endpoint& remote, size_t* bytes = nullptr)
  {
    assert(data);
    if (is_tcp_socket()) {
      return error::operation_not_supported;
    }
    error_code ec;
    size_t n = parent::send_to(buffer(data, size), remote, 0, ec);
    if (bytes) *bytes = n;
    return ec;
  }

  void async_send_to(const char* data, size_t size, const udp::endpoint& remote)
  {
    assert(data);
    async_send_to(data, size, remote, [](const error_code&, size_t) {});
  }

public:
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
      parent::async_wait(what, handler);
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

  template <typename Handler>
  void async_send_to(const char* data, size_t size, const address_t& remote, Handler&& handler)
  {
    assert(data);
    error_code ec;
    auto peer = resolve(remote.addr.c_str(), remote.port, ec);
    if (ec) {
      service()->post([ec, handler]() {
        handler(ec, 0);
      });
      return;
    }
    async_send_to(data, size, *peer.begin(), handler);
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



#pragma once

/********************************************************************************/

#include "ios_ssl.h"

/********************************************************************************/

/********************************************************************************/
namespace asio {
/********************************************************************************/
namespace ip {
/********************************************************************************/

class socket final : public udp::socket {
  typedef udp::socket parent;
  typedef ip::udp::resolver::results_type results_type;

  socket(io::service::value ios)
    : _ios(ios)
    , parent(*ios) {
  }

  socket(io::service::value ios, openssl::context::value ctx)
    : _ios(ios)
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
  typedef openssl::context ssl_context;

  static value create(io::service::value ios) {
    return value(new socket(ios));
  }

  static value create(io::service::value ios, ssl_context::value ctx) {
    return value(new socket(ios, ctx));
  }

  inline io::service::value service() const {
    return _ios;
  }

  inline int id() const {
    return (int)_id.value();
  }

  inline ssl_context::value context() const {
    return _context;
  }

  void close(bool linger = false) {
    /*udp socket*/
    if (is_open()) {
      error_code ec;
      parent::close(ec);
    }
    /*acceptor*/
    if (_acceptor) {
      _acceptor->close();
    }
    /*tcp socket*/
    if (_socket) {
      linger ? _socket->async_close() : _socket->close();
    }
  }

  void assign(ssl_context::value ctx) {
    /*tcp socket only*/
    assert(!is_open());
    assert(!_acceptor);
    if (!_socket) {
      init_socket(ctx);
      return;
    }
    _context = ctx;
    _socket->assign(ctx);
  }

  error_code bind(unsigned short port, const char* host = nullptr) {
    /*udp socket only*/
    assert(!is_open());
    assert(!_socket);
    assert(!_acceptor);
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

  error_code listen(unsigned short port, const char* host = nullptr, int backlog = 16) {
    /*acceptor only*/
    assert(!is_open());
    assert(!_socket);
    assert(!_acceptor);
    init_acceptor();
    return _acceptor->listen(port, host, backlog);
  }

  error_code connect(const char* host, unsigned short port) {
    /*tcp or udp socket*/
    assert(!is_open());
    assert(!_acceptor);
    if (is_open()) {
      error_code ec;
      auto remote = resolve(host, port, ec);
      if (!ec) {
        parent::connect(*remote.begin(), ec);
      }
      return ec;
    }
    init_socket(_context);
    return _socket->connect(host, port);
  }

  error_code local_address(address_t& local) {
    /*tcp/udp/acceptor socket*/
    error_code ec;
    if (is_open()) {
      udp::endpoint end;
      end = parent::local_endpoint(ec);
      if (!ec) {
        local.addr = end.address().to_string();
        local.port = end.port();
      }
      return ec;
    }
    if (_socket) {
      tcp::endpoint end;
      end = _socket->local_endpoint(ec);
      if (!ec) {
        local.addr = end.address().to_string();
        local.port = end.port();
      }
      return ec;
    }
    if (_acceptor) {
      tcp::endpoint end;
      end = _acceptor->local_endpoint(ec);
      if (!ec) {
        local.addr = end.address().to_string();
        local.port = end.port();
      }
      return ec;
    }
    return error::not_socket;
  }

  error_code remote_address(address_t& remote) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    error_code ec;
    if (is_open()) {
      udp::endpoint end;
      end = parent::remote_endpoint(ec);
      if (!ec) {
        remote.addr = end.address().to_string();
        remote.port = end.port();
      }
      return ec;
    }
    if (_socket) {
      tcp::endpoint end;
      end = _socket->remote_endpoint(ec);
      if (!ec) {
        remote.addr = end.address().to_string();
        remote.port = end.port();
      }
      return ec;
    }
    return error::not_socket;
  }

  error_code available(size_t* size) const {
    /*tcp or udp socket*/
    assert(!_acceptor);
    error_code ec;
    size_t n = 0;
    if (_socket) {
      n = _socket->available(ec);
    }
    else if (is_open()) {
      n = parent::available(ec);
    }
    if (size) *size = n;
    return ec;
  }

  error_code accept(socket::value peer) {
    /*acceptor only*/
    assert(peer);
    assert(_acceptor);
    assert(!peer->is_open());
    peer->init_socket();
    return _acceptor->accept(peer->_socket);
  }

  error_code wait(wait_type what) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    if (_socket) {
      return _socket->wait(what);
    }
    error_code ec;
    if (is_open()) {
      parent::wait(what, ec);
    }
    return ec;
  }

  error_code receive(char* data, size_t size, size_t* bytes) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    error_code ec;
    size_t n = 0;
    if (_socket) {
      n = _socket->read_some(buffer(data, size), ec);
    }
    else if (is_open()) {
      n = parent::receive(buffer(data, size), 0, ec);
    }
    if (bytes) *bytes = n;
    return ec;
  }

  error_code receive_from(char* data, size_t size, address_t& remote, size_t* bytes) {
    /*udp socket only*/
    udp::endpoint peer;
    error_code ec = receive_from(data, size, peer, bytes);
    if (!ec) {
      remote.addr = peer.address().to_string();
      remote.port = peer.port();
    }
    return ec;
  }

  error_code receive_from(char* data, size_t size, udp::endpoint& remote, size_t* bytes) {
    /*udp socket only*/
    assert(is_open());
    assert(!_socket);
    assert(!_acceptor);
    error_code ec;
    size_t n = parent::receive_from(buffer(data, size), remote, 0, ec);
    if (bytes) *bytes = n;
    return ec;
  }

  error_code send(const char* data, size_t size, size_t* bytes) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    error_code ec;
    size_t n = 0;
    if (_socket) {
      n = write(*_socket, buffer(data, size), ec);
    }
    else if (is_open()) {
      n = parent::send(buffer(data, size), 0, ec);
    }
    if (bytes) *bytes = n;
    return ec;
  }

  error_code send_to(const char* data, size_t size, const address_t& remote, size_t* bytes) {
    /*udp socket only*/
    error_code ec;
    auto peer = resolve(remote.addr.c_str(), remote.port, ec);
    if (ec) {
      return ec;
    }
    return send_to(data, size, *peer.begin(), bytes);
  }

  error_code send_to(const char* data, size_t size, const udp::endpoint& remote, size_t* bytes) {
    /*udp socket only*/
    assert(is_open());
    assert(!_socket);
    assert(!_acceptor);
    error_code ec;
    size_t n = parent::send_to(buffer(data, size), remote, 0, ec);
    if (bytes) *bytes = n;
    return ec;
  }

public:
  template <typename Handler>
  void async_accept(socket::value peer, Handler&& handler) {
    /*acceptor only*/
    assert(peer);
    assert(_acceptor);
    assert(!peer->is_open());
    peer->init_socket();
    _acceptor->async_accept(peer->_socket, handler);
  }

  template <typename Handler>
  void async_connect(const char* host, unsigned short port, Handler&& handler) {
    /*tcp socket only*/
    assert(!is_open());
    assert(!_acceptor);
    init_socket(_context);
    _socket->async_connect(host, port, handler);
  }

  template <typename Handler>
  void async_wait(wait_type what, Handler&& handler) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    if (_socket) {
      _socket->async_wait(what, handler);
      return;
    }
    if (is_open()) {
      parent::async_wait(what, handler);
    }
  }

  template <typename Handler>
  void async_receive(char* data, size_t size, Handler&& handler) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    if (_socket) {
      _socket->async_read_some(buffer(data, size), handler);
      return;
    }
    if (is_open()) {
      parent::async_receive(buffer(data, size), handler);
    }
  }

  template <typename Handler>
  void async_receive_from(char* data, size_t size, udp::endpoint& remote, Handler&& handler) {
    /*udp socket only*/
    assert(is_open());
    assert(!_socket);
    assert(!_acceptor);
    parent::async_receive_from(buffer(data, size), remote, handler);
  }

  template <typename Handler>
  void async_send(const char* data, size_t size, Handler&& handler) {
    /*tcp or udp socket*/
    assert(!_acceptor);
    if (_socket) {
      _socket->async_send(data, size, handler);
      return;
    }
    if (is_open()) {
      parent::async_send(buffer(data, size), handler);
    }
  }

  template <typename Handler>
  void async_send_to(const char* data, size_t size, const udp::endpoint& remote, Handler&& handler) {
    /*udp socket only*/
    assert(is_open());
    assert(!_socket);
    assert(!_acceptor);
    parent::async_send_to(buffer(data, size), remote, handler);
  }
};

/********************************************************************************/
} //end of namespace ip
/********************************************************************************/
} //end of namespace asio
/********************************************************************************/

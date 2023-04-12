

#pragma once

/********************************************************************************/

#include "ios.h"
#include <asio/ssl.hpp>
#include <functional>

/********************************************************************************/
namespace openssl {
/********************************************************************************/

typedef ssl::stream_base::handshake_type handshake_type;

class context : public ssl::context {
  typedef ssl::context parent;

private:
  context(method what = ssl::context::tlsv12)
    : parent(what) {
    set_options(default_workarounds
      | no_sslv2
      | no_sslv3
      | no_tlsv1
      | single_dh_use
    );
  }

  error_code set_password(const std::string& pwd) {
    error_code ec;
    parent::set_password_callback(
      std::bind([pwd](size_t, password_purpose) {
        return pwd.c_str();
        }, std::placeholders::_1, std::placeholders::_2), ec
    );
    return ec;
  }

public:
  template <typename Handler>
  void use_sni_callback(Handler handler) {
    SSL_CTX_set_tlsext_servername_callback(
      native_handle(),
      [handler](SSL* ssl, int* al, void* arg) {
        handler(SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name));
      }
    );
  }

  error_code load_verify_file(const char* filename) {
    assert(filename);
    error_code ec;
    parent::load_verify_file(filename, ec);
    if (!ec) {
      set_verify_mode(ssl::verify_peer);
      set_verify_mode(ssl::verify_fail_if_no_peer_cert);
    }
    return ec;
  }

  error_code use_certificate_chain(const char* cert, size_t size) {
    assert(cert && size);
    error_code ec;
    set_default_verify_paths(ec);
    if (!ec) {
      parent::use_certificate_chain(buffer(cert, size), ec);
    }
    return ec;
  }

  error_code use_private_key(const char* key, size_t size, const char* pwd = nullptr) {
    assert(key && size);
    error_code ec;
    parent::use_private_key(buffer(key, size), ssl::context::pem, ec);
    if (pwd && !ec) {
      ec = set_password(pwd);
    }
    return ec;
  }

  typedef std::shared_ptr<context> value;
  static value create() { return value(new context()); }
};

/********************************************************************************/

typedef ip::tcp::resolver::results_type results_type;

static results_type resolve(
  const char* host, unsigned short port, error_code& ec) {
  char service[8];
  sprintf(service, "%u", port);
  if (!host || !host[0]) {
    host = "::";
  }
  io_context ios;
  ip::tcp::resolver resolver(ios);
  return resolver.resolve(host, service, ec);
}

/********************************************************************************/

typedef std::function<
  void(const error_code&, size_t)
> callback_t;

class socket : public ip::tcp::socket
  , public std::enable_shared_from_this<socket> {
  typedef ip::tcp::socket parent;
  typedef ssl::stream<parent&> ssl_stream;

  typedef struct {
    std::string data;
    callback_t handler;
  } cache_node;
  friend class acceptor;

  bool _sending = false;
  bool _closing = false;
  char _prevrcv[8192];
  std::list<cache_node> _sendqueue;
  std::shared_ptr<ssl_stream> _stream;

private:
  inline socket(io::service::value ios)
    : parent(*ios)
    , _stream(nullptr) {
  }

  inline socket(io::service::value ios, context::value ctx)
    : parent(*ios)
    , _stream(nullptr)
  {
    assign(ctx);
  }

  socket(const socket&) = delete;

  error_code handshake(handshake_type what) {
    error_code ec;
    _stream ? _stream->handshake(what, ec) : void();
    return ec;
  }

  void set_server_name(const char* server_name) {
    if (_stream) {
      auto handle = _stream->native_handle();
      SSL_set_tlsext_host_name(handle, server_name);
    }
  }

  void on_wait(const error_code& ec, const callback_t& handler) {
    handler(ec, available());
  }

public:
  typedef std::shared_ptr<socket> value;
  virtual ~socket() {
    close();
  }

  static value create(io::service::value ios) {
    return value(new socket(ios));
  }

  static value create(io::service::value ios, context::value ctx) {
    return value(new socket(ios, ctx));
  }

  void assign(context::value ctx) {
    if (!_stream) {
      _stream.reset(new ssl_stream(*this, *ctx));
      return;
    }
    auto handle = _stream->native_handle();
    SSL_set_SSL_CTX(handle, ctx->native_handle());
  }

  size_t read_some(char* buff, size_t size, error_code& ec) {
    return read_some(buffer(buff, size), ec);
  }

  size_t write_some(const char* data, size_t size, error_code& ec) {
    return write_some(buffer(data, size), ec);
  }

  void async_close() {
    post(get_executor(),
      std::bind(&socket::on_async_close, shared_from_this())
    );
  }

  void close() {
    error_code ec;
    if (is_open()) {
      parent::shutdown(shutdown_type::shutdown_both, ec);
      parent::close(ec);
    }
    _closing = false;
  }

  error_code wait(wait_type what) {
    error_code ec;
    parent::wait(what, ec);
    return ec;
  }

  error_code shutdown(shutdown_type what) {
    error_code ec;
    parent::shutdown(what, ec);
    return ec;
  }

  error_code connect(const endpoint_type& remote) {
    assert(!is_open());
    error_code ec;
    parent::connect(remote, ec);
    ec = ec ? ec : handshake(handshake_type::client);
    return ec;
  }

  error_code connect(const char* host, unsigned short port) {
    error_code ec;
    auto remote = resolve(host, port, ec);
    if (!ec) {
      set_server_name(host);
      auto iter = remote.begin();
      for (; iter != remote.end(); iter++) {
        if (!(ec = connect(*iter))) {
          break;
        }
      }
    }
    return ec;
  }

public:
  template <typename Handler>
  void async_connect(const endpoint_type& peer, Handler&& handler) {
    assert(!is_open());
    parent::async_connect(peer,
      [handler](const error_code& ec) {
        ec ? handler(ec) : async_handshake(handshake_type::client, handler);
      }
    );
  }

  template <typename Handler>
  void async_connect(const char* host, unsigned short port, Handler&& handler) {
    assert(!is_open());
    assert(host && port);
    error_code ec;
    auto remote = resolve(host, port, ec);
    if (ec) {
      post(get_executor(), handler);
      return;
    }
    set_server_name(host);
    async_connect(remote, remote.begin(), handler);
  }

  template <typename Handler>
  void async_wait(wait_type what, Handler&& handler) {
    if (!_stream) {
      parent::async_wait(what,
        std::bind(
          &socket::on_wait, shared_from_this(), std::placeholders::_1, (callback_t)handler
        )
      );
      return;
    }
    //async_wait has a bug for ssl socket
    async_read_some(_prevrcv, sizeof(_prevrcv), handler);
  }

  template <typename Handler>
  void async_send(const std::string& data, Handler&& handler) {
    post(get_executor(),
      std::bind(&socket::on_async_send, shared_from_this(), data, (callback_t)handler)
    );
  }

  template <typename Handler>
  void async_send(const char* data, size_t size, Handler&& handler) {
    assert(data);
    const std::string packet(data, size);
    async_send(packet, handler);
  }

  template<typename MutableBufferSequence>
  size_t read_some(const MutableBufferSequence& buffers, error_code& ec) {
    if (!_stream) {
      return parent::read_some(buffers, ec);
    }
    size_t bytes = buffers.size();
    assert(bytes <= sizeof(_prevrcv));
    memcpy(buffers.data(), _prevrcv, bytes);
    return bytes;
  }

  template<typename ConstBufferSequence>
  size_t write_some(const ConstBufferSequence& buffers, error_code& ec) {
    return _stream ? _stream->write_some(buffers, ec) : parent::write_some(buffers, ec);
  }

  template<typename MutableBufferSequence, typename Handler>
  void async_read_some(const MutableBufferSequence& buffers, Handler&& handler) {
    _stream ? _stream->async_read_some(buffers, handler) : parent::async_read_some(buffers, handler);
  }

  template<typename Handler>
  void async_read_some(char* buff, size_t size, Handler&& handler) {
    assert(buff);
    async_read_some(buffer(buff, size), handler);
  }

  template<typename ConstBufferSequence, typename Handler>
  void async_write_some(const ConstBufferSequence& buffers, Handler&& handler) {
    _stream ? _stream->async_write_some(buffers, handler) : parent::async_write_some(buffers, handler);
  }

  template<typename Handler>
  void async_write_some(const char* data, size_t size, Handler&& handler) {
    assert(data);
    async_write_some(buffer(data, size), handler);
  }

private:
  void async_flush() {
    const std::string& packet = _sendqueue.front().data;
    const char* data = packet.c_str();
    size_t size = packet.size();

    async_write(*this,
      buffer(data, size),
      std::bind(&socket::on_async_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2)
    );
    _sending = true;
  }

  void on_async_send(std::string& data, const callback_t& handler) {
    if (_closing) {
      return;
    }
    if (!is_open()) {
      return;
    }
    cache_node _node;
    _node.handler = handler;
    _node.data    = std::move(data);
    _sendqueue.push_back(_node);

    if (!_sending) {
      async_flush();  //flush send cache
    }
  }

  void on_async_close() {
    if (_closing) {
      return;
    }
    if (!is_open()) {
      return;
    }
    error_code ec;
    parent::shutdown(shutdown_type::shutdown_receive, ec);
    if (ec) {
      close();
      return;
    }
    _closing = true;
  }

  void on_async_write(const error_code& ec, size_t transbytes) {
    _sendqueue.front().handler(ec, transbytes);
    _sendqueue.pop_front();
    if (!ec && !_sendqueue.empty()) {
      async_flush();  //flush send queue
      return;
    }
    if (_closing) {
      close();
    }
    _sending = false;
    _sendqueue.clear();
  }

  template <typename Handler>
  void async_handshake(handshake_type what, Handler&& handler) {
    _stream ? _stream->async_handshake(what, handler) : dispatch(
      get_executor(),
      std::bind([handler](const error_code& ec) { handler(ec); }, error_code())
    );
  }

  template <typename Handler>
  void async_connect(const results_type& remote, results_type::const_iterator& iter, Handler&& handler) {
    async_connect(*iter,
      [remote, iter, handler](const error_code& ec) {
        if (!ec) {
          handler(ec);
          return;
        }
        async_connect(remote, ++iter, handler);
      }
    );
  }
};

/********************************************************************************/

class acceptor : public ip::tcp::acceptor {
  typedef ip::tcp::acceptor parent;
  inline acceptor(io::service::value ios)
    : parent(*ios) {
  }

  error_code bind(const endpoint_type& local) {
    error_code ec = open(local.protocol());
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
    parent::bind(local, ec);
    return ec;
  }

  error_code open(const protocol_type& protocol) {
    error_code ec;
    parent::open(protocol, ec);
    return ec;
  }

  acceptor(const acceptor&) = delete;

public:
  typedef std::shared_ptr<acceptor> value;
  virtual ~acceptor() {
    close();
  }

  static value create(io::service::value ios) {
    return value(new acceptor(ios));
  }

  error_code close() {
    error_code ec;
    if (is_open()) parent::close(ec);
    return ec;
  }

  error_code accept(socket::value peer) {
    error_code ec;
    parent::accept(*peer, ec);
    if (!ec) {
      ec = peer->handshake(handshake_type::server);
    }
    return ec;
  }

  error_code listen(const endpoint_type& local, int backlog = 16) {
    error_code ec = bind(local);
    if (!ec) {
      parent::listen(backlog);
    }
    return ec;
  }

  error_code listen(unsigned short port, const char* host = nullptr, int backlog = 16) {
    error_code ec;
    auto local = resolve(host, port, ec);
    if (!ec) {
      ec = listen(*local.begin(), backlog);
    }
    return ec;
  }

  template <typename Handler>
  void async_accept(socket::value peer, Handler&& handler) {
    parent::async_accept(*peer,
      std::bind([handler](const error_code& ec, socket::value peer) {
        ec ? handler(ec) : peer->async_handshake(handshake_type::server, handler);
      }, std::placeholders::_1, peer)
    );
  }
};

/********************************************************************************/
} //end of namespace openssl
/********************************************************************************/

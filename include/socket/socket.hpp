
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

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED
#endif

#include <memory>
#include <functional>
#include <asio.hpp> /* include asio c++ library */
#include <os/os.h>

#ifdef TLS_SSL_ENABLE
#include <asio/ssl.hpp>
#endif

#include "identifier.h"
#include "decoder.h"
#include "circular_buffer.h"

/*******************************************************************************/

#define async_send_timeout  60000  //milliseconds
#define async_send_size     16384  //bytes
#define placeholders1 std::placeholders::_1
#define placeholders2 std::placeholders::_2
#define placeholders3 std::placeholders::_3
#define placeholders4 std::placeholders::_4

/*******************************************************************************/

namespace eth
{
  using namespace asio;

  class socket;

  class reactor : public io_context
    , public std::enable_shared_from_this<reactor> {
  public:
    typedef std::shared_ptr<reactor> ref;

  private:
    inline reactor()
      : _work_guard(make_work_guard(*this)) {
    }
    inline void showerror(const std::exception& e, const char* wheres) {
      //printf("reactor %s error: %s\n", wheres, e.what());
    }
    typedef executor_work_guard<io_context::executor_type> io_work_guard;

  public:
    inline static ref create() { return ref(new reactor()); }

    inline int id() const { return _id.value(); }

    template <typename Handler> inline void post(Handler&& handler) {
      asio::post(*this, handler);
    }
    template <typename Handler> inline void dispatch(Handler&& handler) {
      asio::dispatch(*this, handler);
    }

  private:
    const identifier _id;
    io_work_guard    _work_guard;
  };

  typedef reactor::ref reactor_type;

  typedef std::function<void(const error_code&, size_t)> handler_t;

  namespace udp
  {
    static ip::udp::resolver::results_type resolve(const char* host, unsigned short port, error_code& ec)
    {
      char service[8];
      sprintf(service, "%u", port);
      if (!host || !host[0]) {
        host = "::";
      }
      io_context ios;
      ip::udp::resolver resolver(ios);
      return resolver.resolve(host, service, ec);
    }

    class socket : public ip::udp::socket
      , public std::enable_shared_from_this<socket>
    {
      friend class eth::socket;
      typedef ip::udp::socket parent;
      typedef std::shared_ptr<socket> ref;

      inline socket(reactor_type ios)
        : parent(*ios), _ios(ios) {
      }

      inline socket& operator=(socket&& r) noexcept
      {
        parent::operator=(std::move(r));
        _remote = r._remote;
        return *this;
      }

      inline static ref create(reactor_type ios)
      {
        return ref(new socket(ios));
      }

      void shutdown(bool linger)
      {
        error_code ec;
        parent::close(ec);
      }

      template <typename results_type>
      error_code connect(const results_type& peers, endpoint_type& peer)
      {
        error_code ec;
        peer = asio::connect(*this, peers, ec);
        return ec;
      }

      void dequeue(const error_code& ec, bool keep_on, handler_t handler)
      {
        error_code _ec;
        size_t bytes = 0;
        if (ec) {
          _ec = ec;
        }
        else {
          bytes = available(_ec);
        }
        if (_ec || bytes > 0) {
          handler(_ec, bytes);
        }
        if (keep_on && !_ec) {
          async_wait(socket::wait_read, handler);
        }
      }

      void commit(const error_code& ec, size_t bytes, handler_t handler)
      {
        _notify ? _notify(ec, bytes) : handler(ec, bytes);
      }

      void write(const std::string& data, handler_t handler)
      {
        enqueue(data.c_str(), data.size(), handler);
      }

      void write_to(const std::string& data, const endpoint_type& peer, handler_t handler)
      {
        enqueue(data.c_str(), data.size(), peer, handler);
      }

      void enqueue(const char* data, size_t size, handler_t handler)
      {
        parent::async_send(
          buffer(data, size),
          std::bind(
            &socket::commit, shared_from_this(), placeholders1, placeholders2, handler
          )
        );
      }

      void enqueue(const char* data, size_t size, const endpoint_type& peer, handler_t handler)
      {
        parent::async_send_to(
          buffer(data, size),
          peer,
          std::bind(
            &socket::commit, shared_from_this(), placeholders1, placeholders2, handler
          )
        );
      }

    private:
      reactor_type  _ios;
      handler_t     _notify;
      endpoint_type _remote;

    public:
      inline void close(bool linger = true)
      {
        service()->post(
          std::bind(
            &socket::shutdown, shared_from_this(), linger
          )
        );
      }

      inline void cancel()
      {
        error_code ec;
        parent::cancel(ec);
      }

      inline reactor_type service() const
      {
        return _ios;
      }

      inline error_code assign(int s)
      {
        error_code ec;
        parent::assign(ip::udp::v6(), s, ec);
        return ec;
      }

      inline error_code wait(wait_type what)
      {
        error_code ec;
        parent::wait(what, ec);
        return ec;
      }

      inline size_t timeout() const
      {
        return 0;
      }

      inline void timeout(size_t milliseconds)
      {
        //no used
      }

      inline int native_handle()
      {
        return (int)parent::native_handle();
      }

      inline bool non_blocking() const
      {
        return parent::non_blocking();
      }

      inline error_code non_blocking(bool enable)
      {
        error_code ec;
        parent::non_blocking(enable, ec);
        return ec;
      }

      inline error_code local_endpoint(endpoint_type& local) const
      {
        error_code ec;
        local = parent::local_endpoint(ec);
        return ec;
      }

      inline error_code remote_endpoint(endpoint_type& remote) const
      {
        error_code ec;
        remote = parent::remote_endpoint(ec);
        if (ec) {
          if (_remote.port())
          {
            remote = _remote;
            ec = error_code();
          }
        }
        return ec;
      }

      error_code bind(const endpoint_type& local)
      {
        error_code ec;
        open(local.protocol(), ec);
        if (ec) {
          return ec;
        }
#ifndef _MSC_VER
        reuse_address option(true);
        parent::set_option(option, ec);
#endif
        parent::bind(local, ec);
        return ec;
      }

      error_code bind(unsigned short port, const char* host)
      {
        error_code ec;
        auto locals = resolve(host, port, ec);
        if (ec) {
          return ec;
        }
        return bind(*locals.begin());
      }

      error_code connect(const endpoint_type& peer, size_t reserved)
      {
        error_code ec;
        if (!is_open()) {
          open(peer.protocol(), ec);
        }
        if (!ec) {
          parent::connect(peer, ec);
        }
        return ec;
      }

      error_code connect(const char* host, unsigned short port, size_t reserved)
      {
        assert(host && port);
        error_code ec;
        auto peers = resolve(host, port, ec);
        if (ec) {
          return ec;
        }
        endpoint_type peer;
        return connect(peers, peer);
      }

      size_t send(const char* data, size_t size)
      {
        error_code ec;
        return send(data, size, ec);
      }

      size_t send(const char* data, size_t size, error_code& ec)
      {
        assert(data && size);
        return parent::send(buffer(data, size), 0, ec);
      }

      size_t send_to(const char* data, size_t size, const endpoint_type& peer)
      {
        error_code ec;
        return send_to(data, size, peer, ec);
      }

      size_t send_to(const char* data, size_t size, const endpoint_type& peer, error_code& ec)
      {
        assert(data && size);
        return parent::send_to(buffer(data, size), peer, 0, ec);
      }

      size_t receive(char* buf, size_t size)
      {
        error_code ec;
        return receive_from(buf, size, _remote, ec);
      }

      size_t receive(char* buf, size_t size, error_code& ec)
      {
        assert(buf);
        return receive_from(buf, size, _remote, ec);
      }

      size_t receive_from(char* buf, size_t size, endpoint_type& peer)
      {
        error_code ec;
        return receive_from(buf, size, peer, ec);
      }

      size_t receive_from(char* buf, size_t size, endpoint_type& peer, error_code& ec)
      {
        assert(buf);
        return parent::receive_from(buffer(buf, size), peer, 0, ec);
      }

      void async_send(const std::string& data)
      {
        async_send(data, [](const error_code&, size_t) {});
      }

      void async_send(const char* data, size_t bytes)
      {
        async_send(data, bytes, [](const error_code&, size_t) {});
      }

      void async_send_to(const std::string& data, const endpoint_type& peer)
      {
        async_send_to(data, peer, [](const error_code&, size_t) {});
      }

      void async_send_to(const char* data, size_t bytes, const endpoint_type& peer)
      {
        async_send_to(data, bytes, peer, [](const error_code&, size_t) {});
      }

    public:
      template <typename Handler>
      void async_connect(const endpoint_type& peer, Handler&& handler)
      {
        assert(!is_open());
        parent::async_connect(peer, handler);
      }

      template <typename Handler>
      void async_connect(const char* host, unsigned short port, Handler&& handler)
      {
        assert(host && port);
        error_code ec;
        auto peers = resolve(host, port, ec);
        if (ec) {
          service()->post([=]() { handler(ec); });
          return;
        }
        asio::async_connect(
          *this, peers, [handler](const error_code& ec, const endpoint_type&) {
            handler(ec);
          }
        );
      }

      template <typename Option>
      inline error_code set_option(const Option& option)
      {
        error_code ec;
        parent::set_option(option, ec);
        return ec;
      }

      template <typename Handler>
      void async_send(const std::string& data, Handler&& handler)
      {
        service()->post(
          std::bind(
            &socket::write, shared_from_this(), data, (handler_t)handler
          )
        );
      }

      template <typename Handler>
      void async_send(const char* data, size_t size, Handler&& handler)
      {
        std::string packet(data, size);
        async_send(packet, handler);
      }

      template <typename Handler>
      void async_send_to(const std::string& data, const endpoint_type& peer, Handler&& handler)
      {
        service()->post(
          std::bind(
            &socket::write_to, shared_from_this(), data, peer, (handler_t)handler
          )
        );
      }

      template <typename Handler>
      void async_send_to(const char* data, size_t size, const endpoint_type& peer, Handler&& handler)
      {
        std::string packet(data, size);
        async_send_to(packet, peer, handler);
      }

      template <typename Handler>
      void async_wait(wait_type what, Handler&& handler)
      {
        async_wait(what, true, handler);
      }

      template <typename Handler>
      void async_wait(wait_type what, bool keep_on, Handler&& handler)
      {
        if (what == socket::wait_write) {
          _notify = handler;
          return;
        }
        parent::async_wait(
          wait_read,
          std::bind(
            &socket::dequeue, shared_from_this(), placeholders1, keep_on, (handler_t)handler
          )
        );
      }
    };
  } //end of namespace udp

  namespace tls
  {
#ifdef TLS_SSL_ENABLE
    typedef ssl::context ssl_context;
    typedef ssl::stream_base::handshake_type handshake_type;

    class socket : public ip::tcp::socket
    {
      typedef ip::tcp::socket parent;
      typedef ssl::stream<socket&> ssl_stream;
      ssl_stream *_stream;

      template <typename Handler>
      inline void handshake(Handler&& handler)
      {
        error_code ec;
        post(
          get_executor(),
          std::bind([handler](const error_code& ec) { handler(ec); }, ec)
        );
      }

    public:
      template <typename ExecutionContext>
      inline explicit socket(ExecutionContext& context)
        : parent(context)
        , _stream(nullptr) {
      }

      virtual ~socket() {
        delete _stream;
      }

      inline SSL* ssl_handle() const {
        return _stream ? _stream->native_handle() : nullptr;
      }

      inline void ssl_enable(ssl_context& sslctx) {
        _stream = new ssl_stream(*this, sslctx);
      }

      inline void handshake(handshake_type type, error_code& ec) {
        _stream ? _stream->handshake(type, ec) : ec.clear();
      }

      template <typename Handler>
      inline void async_handshake(handshake_type type, Handler&& handler) {
        _stream ? _stream->async_handshake(type, handler) : handshake(handler);
      }

      template <typename MutableBuffers>
      inline size_t receive(const MutableBuffers& buffers, int flag, error_code& ec) {
        return _stream ? _stream->read_some(buffers, ec) : read_some(buffers, ec);
      }

      template <typename MutableBuffers, typename Handler>
      inline void async_receive(const MutableBuffers& buffers, Handler&& handler) {
        _stream ? _stream->async_read_some(buffers, handler) : async_read_some(buffers, handler);
      }

      template <typename MutableBuffers>
      inline size_t send(const MutableBuffers& buffers, int flag, error_code& ec) {
        return _stream ? write(*_stream, buffers, ec) : write(*this, buffers, ec);
      }

      template <typename MutableBuffers, typename Handler>
      inline void async_send(const MutableBuffers& buffers, Handler&& handler) {
        _stream ? async_write(*_stream, buffers, handler) : async_write(*this, buffers, handler);
      }
    };
#else
    enum struct handshake_type {
      /// Perform handshaking as a client.
      client,
      /// Perform handshaking as a server.
      server
    };

    class socket : public ip::tcp::socket {
      typedef ip::tcp::socket parent;

    public:
      template <typename ExecutionContext>
      inline explicit socket(ExecutionContext& context)
        : parent(context) {
      }

      inline void handshake(handshake_type type, error_code& ec) {
        ec.clear();
      }

      template <typename Handler>
      inline void async_handshake(handshake_type type, Handler&& handler) {
        error_code ec;
        post(
          get_executor(),
          std::bind([handler](const error_code& ec) { handler(ec); }, ec)
        );
      }

      template <typename MutableBuffers>
      inline size_t send(const MutableBuffers& buffers, int flag, error_code& ec) {
        return write(*this, buffers, ec);
      }

      template <typename MutableBuffers, typename Handler>
      inline void async_send(const MutableBuffers& buffers, Handler&& handler) {
        async_write(*this, buffers, handler);
      }
    };
#endif
  } //end of namespace tls

  namespace tcp
  {
    static ip::tcp::resolver::results_type resolve(const char* host, unsigned short port, error_code& ec)
    {
      char service[8];
      sprintf(service, "%u", port);
      if (!host || !host[0]) {
        host = "::";
      }
      io_context ios;
      ip::tcp::resolver resolver(ios);
      return resolver.resolve(host, service, ec);
    }

    class socket : public tls::socket
      , public std::enable_shared_from_this<socket>
    {
      friend class eth::socket;
      typedef tls::socket parent;
      typedef std::shared_ptr<socket> ref;

      inline explicit socket(reactor_type ios)
        : parent(*ios)
        , _timer(*ios)
        , _ios  ( ios)
        , _acceptor(0)
        , _tmsend  (0)
        , _tmrecv  (0)
        , _expires (0)
        , _widx    (0)
        , _closed (false)
        , _sending(false) {
      }
#if 0
      inline socket& operator=(socket&& r) noexcept
      {
        assert(!is_open());
        assert(!r._acceptor);
        assert(!r._tmsend && !r._tmrecv);
        parent::operator=(std::move(r));
        return *this;
      }
#endif
      inline static ref create(reactor_type ios)
      {
        return ref(new socket(ios));
      }

      inline circular_buffer& cache(int index)
      {
        return _buffers[index];
      }

      void shutdown(bool linger)
      {
        error_code ec;
        if (_acceptor)
        {
          _acceptor->close(ec);
          delete _acceptor;
          _acceptor = 0;
          return;
        }
        if (_expires > 0) {
          _timer.cancel();
        }
        if (_sending && linger)
        {
          _closed = true;
          parent::shutdown(shutdown_receive, ec);
          return;
        }
        parent::shutdown(shutdown_type::shutdown_send, ec);
        parent::close(ec);
      }

      void block(reactor_type ios, size_t timeout)
      {
        if (timeout < 1000) {
          timeout = 1000;
        }
        if (!ios->run_one_for(std::chrono::milliseconds(timeout)))
        {
          while (!ios->stopped())
          {
            if (is_open()) {
              shutdown(false);
            }
            std::this_thread::sleep_for(
              std::chrono::milliseconds(1)
            );
          }
        }
      }

      error_code connect(const endpoint_type& peer)
      {
        error_code ec;
        parent::connect(peer, ec);
        return ec;
      }

      template <typename results_type>
      error_code connect(const results_type& peers, endpoint_type& peer)
      {
        error_code ec;
        peer = asio::connect(*this, peers, ec);
        return ec;
      }

      void dequeue(const error_code& ec, bool keep_on, handler_t handler)
      {
        error_code _ec;
        size_t bytes = 0;
        if (ec) {
          _ec = ec;
        }
        else {
          bytes = available(_ec);
        }
        if (bytes == 0) {
          _ec = error::connection_reset;
        }
        _tmrecv = 0;
        if (_ec || bytes > 0) {
          handler(_ec, bytes);
        }
        if (keep_on && !_ec) {
          async_wait(socket::wait_read, handler);
        }
      }

      void enqueue(const char* data, size_t size, handler_t handler)
      {
        size_t n = cache(_widx).write(data, size);
        if (n != size) {
          shutdown(false);
          return;
        }
        if (!_sending) {
          flush(0, _widx, handler);
          return;
        }
        if (os::milliseconds() - _tmsend > async_send_timeout) {
          shutdown(false);
        }
      }

      void write(const std::string& data, handler_t handler)
      {
        enqueue(data.c_str(), data.size(), handler);
      }

      void commit(const error_code& ec, size_t bytes, handler_t handler)
      {
        if (!ec)
        {
          int index = 1 - _widx;
          cache(index).erase(bytes);

          if (!cache(index).empty()) {
            _widx = index;
          }
          flush(bytes, _widx, handler);
        }
        if (is_open()) {
          handler(ec, bytes);
        }
      }

      void flush(size_t sent, int index, handler_t handler)
      {
        size_t size = cache(index).size();
        if (size == 0)
        {
          _sending = false;
          if (_closed) {
            shutdown(_sending);
          }
          else if (_notify)
          {
            error_code ec;
            _notify(ec, sent);
          }
          return;
        }
        _sending = true;
        _widx = 1 - index;
        size = _min_size(size, async_send_size);

        parent::async_send(
          buffer(cache(index).data(), size),
          std::bind(
          &socket::commit, shared_from_this(), placeholders1, placeholders2, handler
          )
        );
        _tmsend = os::milliseconds();
      }

      void on_timer(const error_code& ec, size_t interval)
      {
        if (!ec && _expires > 0)
        {
          _tmrecv += interval;
          if (_tmrecv >= _expires)
          {
            shutdown(false);
            return;
          }
          set_timer(interval);
        }
      }

      void set_timer(size_t interval)
      {
        _timer.expires_after(
          std::chrono::milliseconds(interval)
        );
        _timer.async_wait(std::bind(
          &socket::on_timer, shared_from_this(), placeholders1, interval
        ));
      }

    private:
      reactor_type       _ios;
      ip::tcp::acceptor* _acceptor;
      handler_t          _notify;
      asio::steady_timer _timer;
      size_t             _expires;
      size_t             _tmsend;
      size_t             _tmrecv;
      bool               _sending;
      bool               _closed;
      int                _widx;
      circular_buffer    _buffers[2];

    public:
      virtual ~socket()
      {
        delete _acceptor;
      }

      inline reactor_type service() const
      {
        return _ios;
      }

      inline bool is_open() const
      {
        if (!_acceptor) {
          return parent::is_open() && !_closed;
        }
        return _acceptor->is_open();
      }

      inline void close(bool linger = true)
      {
        service()->post(
          std::bind(
            &socket::shutdown, shared_from_this(), linger
          )
        );
      }

      inline void cancel()
      {
        error_code ec;
        parent::cancel(ec);
      }

      inline error_code assign(int s)
      {
        error_code ec;
        parent::assign(ip::tcp::v6(), s, ec);
        return ec;
      }

      inline size_t timeout() const
      {
        return _expires;
      }

      inline void timeout(size_t milliseconds)
      {
        if (_acceptor) {
          return;
        }
        if (_expires == 0 && milliseconds) {
          set_timer(1000);
        }
        if (milliseconds < 1000) {
          milliseconds = 1000;
        }
        _tmrecv = 0;
        _expires = milliseconds;
      }

      inline int native_handle()
      {
        return (int)parent::native_handle();
      }

      inline bool non_blocking() const
      {
        return parent::non_blocking();
      }

      inline error_code non_blocking(bool enable)
      {
        error_code ec;
        parent::non_blocking(enable, ec);
        return ec;
      }

      inline error_code wait(wait_type what)
      {
        error_code ec;
        parent::wait(what, ec);
        return ec;
      }

      inline error_code no_delay()
      {
        error_code ec;
        ip::tcp::no_delay option(true);
        parent::set_option(option, ec);
        return ec;
      }

      inline error_code local_endpoint(endpoint_type& local) const
      {
        error_code ec;
        local = _acceptor ? _acceptor->local_endpoint(ec) : parent::local_endpoint(ec);
        return ec;
      }

      inline error_code remote_endpoint(endpoint_type& remote) const
      {
        error_code ec;
        remote = parent::remote_endpoint(ec);
        return ec;
      }

      error_code connect(const endpoint_type& peer, size_t timeout)
      {
        assert(!_acceptor);
        assert(!is_open());
        error_code ec;
        auto ios = reactor::create();
        std::thread thd([this, ios, timeout]() {
            this->block(ios, timeout);
          }
        );
        ec = connect(peer);
        if (thd.joinable())
        {
          ios->stop();
          thd.join();
        }
        return is_open() ? ec : error::timed_out;
      }

      error_code connect(const char* host, unsigned short port, size_t timeout)
      {
        assert(!_acceptor);
        assert(!is_open());
        assert(host && port);

        error_code ec;
        auto peers = resolve(host, port, ec);
        if (ec) {
          return ec;
        }
        auto ios = reactor::create();
        std::thread thd([this, ios, timeout]() {
            this->block(ios, timeout);
          }
        );
        endpoint_type peer;
        ec = connect(peers, peer);
        if (thd.joinable())
        {
          ios->stop();
          thd.join();
        }
        return is_open() ? ec : error::timed_out;
      }

      error_code listen(const endpoint_type& local, int backlog)
      {
        assert(!_acceptor);
        assert(!is_open());

        _acceptor = new ip::tcp::acceptor(*service());
        if (!_acceptor) {
          return error::no_memory;
        }
        error_code ec;
        _acceptor->open(local.protocol(), ec);
        if (ec) {
          return ec;
        }
#ifndef _MSC_VER
        reuse_address option(true);
        _acceptor->set_option(option, ec);
#endif
        _acceptor->bind(local, ec);
        if (!ec) {
          _acceptor->listen(backlog);
        }
        return ec;
      }

      error_code listen(unsigned short port, const char* host, int backlog)
      {
        error_code ec;
        auto locals = resolve(host, port, ec);
        if (ec) {
          return ec;
        }
        return listen(*locals.begin(), backlog);
      }

      error_code accept(socket::ref peer)
      {
        error_code ec = error::bad_descriptor;
        if (_acceptor && peer) {
          _acceptor->accept(*peer, ec);
        }
        return ec;
      }

      size_t send(const char* data, size_t size)
      {
        assert(data && size);
        error_code ec;
        return send(data, size, ec);
      }

      size_t send(const char* data, size_t size, error_code& ec)
      {
        assert(data && size);
        return parent::send(buffer(data, size), 0, ec);
      }

      size_t receive(char* buf, size_t size)
      {
        assert(buf);
        error_code ec;
        return receive(buf, size, ec);
      }

      size_t receive(char* buf, size_t size, error_code& ec)
      {
        assert(buf);
        return parent::receive(buffer(buf, size), 0, ec);
      }

      void async_send(const std::string& data)
      {
        async_send(data, [](const error_code&, size_t) {});
      }

      void async_send(const char* data, size_t bytes)
      {
        async_send(data, bytes, [](const error_code&, size_t) {});
      }

      template <typename Option>
      inline error_code set_option(const Option& option)
      {
        error_code ec;
        parent::set_option(option, ec);
        return ec;
      }

      template <typename Handler>
      void async_connect(const endpoint_type& peer, Handler&& handler)
      {
        assert(!_acceptor);
        assert(!is_open());
        parent::async_connect(peer, handler);
      }

      template <typename Handler>
      void async_connect(const char* host, unsigned short port, Handler&& handler)
      {
        assert(host && port);
        error_code ec;
        auto peers = resolve(host, port, ec);
        if (ec) {
          service()->post([=]() { handler(ec); });
          return;
        }
        asio::async_connect(
          *this, peers, [handler](const error_code& ec, const endpoint_type&) {
            handler(ec);
          }
        );
      }

      template <typename Handler>
      void async_accept(socket::ref peer, Handler&& handler)
      {
        assert(_acceptor && peer);
        _acceptor->async_accept(*peer, handler);
      }

      template <typename Handler>
      void async_send(const std::string& data, Handler&& handler)
      {
        service()->post(
          std::bind(
            &socket::write, shared_from_this(), data, (handler_t)handler
          )
        );
      }

      template <typename Handler>
      void async_send(const char* data, size_t bytes, Handler&& handler)
      {
        std::string packet(data, bytes);
        async_send(packet, handler);
      }

      template <typename Handler>
      void async_wait(wait_type what, Handler&& handler)
      {
        async_wait(what, true, handler);
      }

      template <typename Handler>
      void async_wait(wait_type what, bool keep_on, Handler&& handler)
      {
        if (what == socket::wait_write) {
          _notify = handler;
          return;
        }
        parent::async_wait(
          wait_read,
          std::bind(
            &socket::dequeue, shared_from_this(), placeholders1, keep_on, (handler_t)handler
          )
        );
      }
    };
  } //end of namespace tcp

  class socket
    : public std::enable_shared_from_this<socket> {
  public:
    typedef std::shared_ptr<socket> ref;
    typedef std::function<void(const error_code&, ref)> Acceptor;

    enum struct family {
      sock_stream,
      sock_dgram
    };

    enum struct wait_type {
      wait_read,
      wait_write
    };

  private:
    socket(reactor_type ios, family mode)
      : _reactor(ios), _context(0), _is_server(true)
    {
      assert(ios);
      switch (mode)
      {
      case family::sock_stream:
        _tcp = tcp::socket::create(ios);
        break;

      case family::sock_dgram:
        _udp = udp::socket::create(ios);
        break;

      default:
        throw("socket type not supported");
      }
    }

    void on_accept(const error_code& ec, ref peer, bool keep_on, Acceptor handler)
    {
      handler(ec, peer);
      if (keep_on && is_open()) {
        async_accept(keep_on, handler);
      }
    }

    const identifier  _id;
    const void*       _context;
    reactor_type      _reactor;
    encoder           _encoder;
    decoder           _decoder;
    bool              _is_server;
    tcp::socket::ref  _tcp; //tcp socket
    udp::socket::ref  _udp; //udp socket

  public:
    inline static ref create(reactor_type ios, family mode)
    {
      assert(ios);
      return ref(new socket(ios, mode));
    }
#if 0
    inline ref swap(ref other)
    {
      if (other->_tcp) {
        *_tcp = std::move(*other->_tcp);
      }
      if (other->_udp) {
        *_udp = std::move(*other->_udp);
      }
      return shared_from_this();
    }
#endif
    inline reactor_type service() const
    {
      return _reactor;
    }

    inline bool is_open() const
    {
      return _tcp ? _tcp->is_open() : _udp->is_open();
    }

    inline int id() const
    {
      return _id.value();
    }

    inline const void* context() const
    {
      return _context;
    }

    inline void context(const void* value)
    {
      _context = value;
    }

    inline void cancel()
    {
      _tcp ? _tcp->cancel() : _udp->cancel();
    }

    inline void close(bool linger = true)
    {
      _tcp ? _tcp->close(linger) : _udp->close(linger);
    }

#ifdef TLS_SSL_ENABLE
    inline void ssl_enable(tls::ssl_context& ctx)
    {
      assert(_tcp);
      _tcp->ssl_enable(ctx);
    }
    inline SSL* ssl_handle() const
    {
      return _tcp ? _tcp->ssl_handle() : nullptr;
    }
#endif

    inline void handshake(error_code& ec)
    {
      assert(_tcp);
      _tcp->handshake((_is_server ? tls::handshake_type::server : tls::handshake_type::client), ec);
    }

    inline size_t timeout() const
    {
      return _tcp ? _tcp->timeout() : _udp->timeout();
    }

    inline void timeout(size_t milliseconds)
    {
      _tcp ? _tcp->timeout(milliseconds) : _udp->timeout(milliseconds);
    }

    inline int native_handle()
    {
      return _tcp ? _tcp->native_handle() : _udp->native_handle();
    }

    inline bool non_blocking() const
    {
      return _tcp ? _tcp->non_blocking() : _udp->non_blocking();
    }

    error_code wait(wait_type what)
    {
      return _tcp ? _tcp->wait((tcp::socket::wait_type)what) : _udp->wait((udp::socket::wait_type)what);
    }

    error_code assign(int handle)
    {
      assert(!is_open());
      return _tcp ? _tcp->assign(handle) : _udp->assign(handle);
    }

    error_code non_blocking(bool enable)
    {
      return _tcp ? _tcp->non_blocking(enable) : _udp->non_blocking(enable);
    }

    error_code bind(const ip::udp::endpoint& local)
    {
      assert(_udp);
      return _udp->bind(local);
    }

    error_code bind(unsigned short port, const ip::address& addr)
    {
      assert(_udp);
      ip::udp::endpoint local(addr, port);
      return _udp->bind(local);
    }

    error_code bind(unsigned short port, const char* host)
    {
      assert(_udp);
      return _udp->bind(port, host);
    }

    error_code listen(const ip::tcp::endpoint& local, int backlog = 16)
    {
      assert(_tcp);
      return _tcp->listen(local, backlog);
    }

    error_code listen(unsigned short port, const ip::address& addr, int backlog = 16)
    {
      assert(_tcp);
      ip::tcp::endpoint local(addr, port);
      return _tcp->listen(local, backlog);
    }

    error_code listen(unsigned short port, const char* host, int backlog = 16)
    {
      assert(_tcp);
      return _tcp->listen(port, host, backlog);
    }

    error_code connect(const ip::udp::endpoint& remote, size_t timeout = 5000)
    {
      assert(_udp);
      _is_server = false;
      return _udp->connect(remote, timeout);
    }

    error_code connect(const ip::tcp::endpoint& remote, size_t timeout = 5000)
    {
      assert(_tcp);
      _is_server = false;
      return _tcp->connect(remote, timeout);
    }

    error_code connect(const ip::address& addr, unsigned short port, size_t timeout = 5000)
    {
      _is_server = false;
      return _tcp ? _tcp->connect(
        ip::tcp::endpoint(addr, port), timeout
      ) : _udp->connect(
        ip::udp::endpoint(addr, port), timeout
      );
    }

    error_code connect(const char* host, unsigned short port, size_t timeout = 5000)
    {
      assert(host && port);
      _is_server = false;
      return _tcp ? _tcp->connect(host, port, timeout) : _udp->connect(host, port, timeout);
    }

    error_code local_address(ip::address& addr, unsigned short* port = 0)
    {
      error_code ec;
      if (_tcp)
      {
        ip::tcp::endpoint local;
        ec = _tcp->local_endpoint(local);
        if (!ec)
        {
          addr = local.address();
          if (port) {
            *port = local.port();
          }
        }
      }
      else
      {
        ip::udp::endpoint local;
        ec = _udp->local_endpoint(local);
        if (!ec)
        {
          addr = local.address();
          if (port) {
            *port = local.port();
          }
        }
      }
      return ec;
    }

    error_code remote_address(ip::address& addr, unsigned short* port = 0)
    {
      error_code ec;
      if (_tcp)
      {
        ip::tcp::endpoint remote;
        ec = _tcp->remote_endpoint(remote);
        if (!ec)
        {
          addr = remote.address();
          if (port) {
            *port = remote.port();
          }
        }
      }
      else
      {
        ip::udp::endpoint remote;
        ec = _udp->remote_endpoint(remote);
        if (!ec)
        {
          addr = remote.address();
          if (port) {
            *port = remote.port();
          }
        }
      }
      return ec;
    }

    error_code accept(socket::ref peer)
    {
      assert(_tcp);
      assert(peer);
      assert(peer->_tcp);
      return _tcp->accept(peer->_tcp);
    }

    error_code no_delay()
    {
      assert(_tcp);
      return _tcp->no_delay();
    }

    size_t send(const char* data, size_t size)
    {
      assert(data);
      return _tcp ? _tcp->send(data, size) : _udp->send(data, size);
    }

    size_t send(const char* data, size_t size, error_code& ec)
    {
      assert(data);
      return _tcp ? _tcp->send(data, size, ec) : _udp->send(data, size, ec);
    }

    size_t send_to(const char* data, size_t size, const ip::udp::endpoint& peer)
    {
      assert(_udp);
      assert(data);
      return _udp->send_to(data, size, peer);
    }

    size_t send_to(const char* data, size_t size, const ip::udp::endpoint& peer, error_code& ec)
    {
      assert(_udp);
      assert(data);
      return _udp->send_to(data, size, peer, ec);
    }

    size_t receive(decoder& buf, size_t size)
    {
      error_code ec;
      return receive(buf, size, ec);
    }

    size_t receive(decoder& buf, size_t size, error_code& ec)
    {
      char data[8192];
      size_t writen = 0;
      size = _min_size(available(), size);

      while (size > 0)
      {
        size_t bytes = sizeof(data);
        bytes = _min_size(bytes, size);
        size_t recved = receive(data, bytes, ec);
        if (ec) {
          break;
        }
        assert(size >= recved);
        size -= recved;
        writen += buf.write(data, recved);
      }
      return writen;
    }

    size_t available() const
    {
      error_code ec;
      size_t n = _tcp ? _tcp->available(ec) : _udp->available(ec);
      return ec ? 0 : n;
    }

    size_t receive(char* buf, size_t size)
    {
      assert(buf);
      return _tcp ? _tcp->receive(buf, size) : _udp->receive(buf, size);
    }

    size_t receive(char* buf, size_t size, error_code& ec)
    {
      assert(buf);
      return _tcp ? _tcp->receive(buf, size, ec) : _udp->receive(buf, size, ec);
    }

    size_t receive_from(char* buf, size_t size, ip::udp::endpoint& peer)
    {
      assert(_udp);
      assert(buf);
      return _udp->receive_from(buf, size, peer);
    }

    size_t receive_from(char* buf, size_t size, ip::udp::endpoint& peer, error_code& ec)
    {
      assert(_udp);
      assert(buf);
      return _udp->receive_from(buf, size, peer, ec);
    }

    void async_send(const std::string& data)
    {
      _tcp ? _tcp->async_send(data) : _udp->async_send(data);
    }

    void async_send(const char* data, size_t bytes)
    {
      assert(data);
      _tcp ? _tcp->async_send(data, bytes) : _udp->async_send(data, bytes);
    }

    void async_send_to(const std::string& data, const ip::address& addr, unsigned short port)
    {
      ip::udp::endpoint peer(addr, port);
      async_send_to(data, peer);
    }

    void async_send_to(const std::string& data, const ip::udp::endpoint& peer)
    {
      async_send_to(data.c_str(), data.size(), peer);
    }

    void async_send_to(const char* data, size_t size, const ip::address& addr, unsigned short port)
    {
      assert(_udp);
      assert(data);
      ip::udp::endpoint peer(addr, port);
      _udp->async_send_to(data, peer);
    }

    void async_send_to(const char* data, size_t bytes, const ip::udp::endpoint& peer)
    {
      assert(_udp);
      assert(data);
      _udp->async_send_to(data, bytes, peer);
    }

  public:
    template <typename Option>
    inline error_code set_option(const Option& option)
    {
      return _tcp ? _tcp->set_option(option) : _udp->set_option(option);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    error_code bind(unsigned short port, const char* host, Handler&& handler)
    {
      assert(_udp);
      error_code ec;
      ec = bind(port, host);
      if (!ec) {
        async_wait(wait_type::wait_read, handler);
      }
      return ec;
    }

    //Handler: void(const error_code& /* ec */, socket_type /* peer */);
    template <typename Handler>
    error_code listen(unsigned short port, const char* host, int backlog, Handler&& handler)
    {
      assert(_tcp);
      error_code ec;
      ec = listen(port, host, backlog);
      if (!ec) {
        async_accept(handler);
      }
      return ec;
    }

    //Handler: void(const error_code& /* ec */);
    template <typename Handler>
    void async_connect(const ip::tcp::endpoint& peer, Handler&& handler)
    {
      assert(_tcp);
      _tcp->async_connect(peer, handler);
    }

    //Handler: void(const error_code& /* ec */);
    template <typename Handler>
    void async_connect(const ip::udp::endpoint& peer, Handler&& handler)
    {
      assert(_udp);
      _udp->async_connect(peer, handler);
    }

    //Handler: void(const error_code& /* ec */);
    template <typename Handler>
    void async_connect(const char* host, unsigned short port, Handler&& handler)
    {
      assert(host && port);
      _tcp ? _tcp->async_connect(host, port, handler) : _udp->async_connect(host, port, handler);
    }

    //Handler: void(const error_code& /* ec */, socket_type /* peer */);
    template <typename Handler>
    void async_accept(Handler&& handler)
    {
      assert(_tcp);
      auto peer = create(service(), family::sock_stream);
      async_accept(peer, true, handler);
    }

    //Handler: void(const error_code& /* ec */, socket_type /* peer */);
    template <typename Handler>
    void async_accept(bool keep_on, Handler&& handler)
    {
      assert(_tcp);
      auto peer = create(service(), family::sock_stream);
      async_accept(peer, keep_on, handler);
    }

    //Handler: void(const error_code& /* ec */, socket_type /* peer */);
    template <typename Handler>
    void async_accept(socket::ref peer, bool keep_on, Handler&& handler)
    {
      assert(_tcp);
      assert(peer->_tcp);

      _tcp->async_accept(
        peer->_tcp,
        std::bind(
          &socket::on_accept, shared_from_this(), placeholders1, peer, keep_on, (Acceptor)handler
        )
      );
    }

    //Handler: void(const error_code& /* ec */);
    template <typename Handler>
    void async_handshake(Handler&& handler)
    {
      assert(_tcp);
      _tcp->async_handshake((_is_server ? tls::handshake_type::server : tls::handshake_type::client), handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_wait(wait_type what, Handler&& handler)
    {
      async_wait(what, true, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_wait(wait_type what, bool keep_on, Handler&& handler)
    {
      _tcp ? _tcp->async_wait(
        (tcp::socket::wait_type)what, keep_on, handler
      ) : _udp->async_wait(
      (udp::socket::wait_type)what, keep_on, handler
      );
    }

    //Handler: void(const char* /* data */, size_t /* size */, const header*);
    template <typename Handler>
    size_t decode(size_t size, int& err, Handler&& handler)
    {
      receive(_decoder, size);
      err = _decoder.decode(handler);
      return _decoder.size();
    }

    //Handler: void(const char* /* data */, size_t /* size */, const header*);
    template <typename Handler>
    size_t decode(const char* data, size_t size, int& err, Handler&& handler)
    {
      _decoder.write(data, size);
      err = _decoder.decode(handler);
      return _decoder.size();
    }

    //Handler: void(const char* /* data */, size_t /* size */);
    template <typename Handler>
    void encode(int opcode, const char* data, size_t size, bool encrypt, bool compress, Handler&& handler)
    {
      assert(opcode);
      _encoder.encode(opcode, data, size, encrypt, compress, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_send(const std::string& data, Handler&& handler)
    {
      _tcp ? _tcp->async_send(data, handler) : _udp->async_send(data, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_send(const char* data, size_t bytes, Handler&& handler)
    {
      assert(data);
      _tcp ? _tcp->async_send(data, bytes, handler) : _udp->async_send(data, bytes, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_send_to(const std::string& data, const ip::address& addr, unsigned short port, Handler&& handler)
    {
      assert(_udp);
      ip::udp::endpoint peer(addr, port);
      async_send_to(data, peer, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_send_to(const std::string& data, const ip::udp::endpoint& peer, Handler&& handler)
    {
      assert(_udp);
      async_send_to(data.c_str(), data.size(), peer, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_send_to(const char* data, size_t bytes, const ip::address& addr, unsigned short port, Handler&& handler)
    {
      assert(_udp);
      assert(data);
      ip::udp::endpoint peer(addr, port);
      _udp->async_send_to(data, bytes, peer, handler);
    }

    //Handler: void(const error_code& /* ec */, size_t /* size */);
    template <typename Handler>
    void async_send_to(const char* data, size_t bytes, const ip::udp::endpoint& peer, Handler&& handler)
    {
      assert(_udp);
      assert(data);
      _udp->async_send_to(data, bytes, peer, handler);
    }
  };

  typedef socket::ref socket_type;
} //end of namespace eth

/*******************************************************************************/

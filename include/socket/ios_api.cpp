

#include <mutex>
#include <map>
#include <memory>

#include "ios_api.h"
#include "ios_socket.h"

/*******************************************************************************/

static std::mutex eport_mutex;
static std::map<eport_handle, io::service::value> eport_handles;
static std::map<eport_handle, ip::socket::value> eport_sockets;
#define thread_lock_begin std::unique_lock<std::mutex> lock(eport_mutex)

/*******************************************************************************/

static io::service::value find_handle(eport_handle handle)
{
  thread_lock_begin;
  auto iter = eport_handles.find(handle);
  return iter == eport_handles.end() ? io::service::value() : iter->second;
}

/*******************************************************************************/

static ip::socket::value find_socket(eport_handle socket)
{
  thread_lock_begin;
  auto iter = eport_sockets.find(socket);
  return iter == eport_sockets.end() ? ip::socket::value() : iter->second;
}

/*******************************************************************************/

template <typename _EndpointType>
static void copy_endpoint(const _EndpointType& end, eport_endpoint& out) {
  out.size = end.size();
  memcpy(out.data, end.data(), out.size);
}

/*******************************************************************************/

template <typename _EndpointType>
static void copy_endpoint(const eport_endpoint& end, _EndpointType& out) {
  memcpy(out.data(), end.data, end.size);
}

/*******************************************************************************/

eport_errno eport_check_version(size_t version)
{
  return version == eport_version ? eport_true : eport_false;
}

/*******************************************************************************/

eport_handle eport_create()
{
  auto ios = io::service::create();
  if (ios) {
    thread_lock_begin;
    eport_handles[ios->id()] = ios;
  }
  return ios ? ios->id() : eport_none;
}

/*******************************************************************************/

eport_errno eport_release(eport_handle handle)
{
  auto ios = find_handle(handle);
  if (ios) {
    thread_lock_begin;
    if (eport_handles.erase(ios->id())) {
      ios->stop();
      return eport_error_ok;
    }
  }
  return error::service_not_found;
}

/*******************************************************************************/

eport_errno eport_post(eport_handle handle, eport_cb_dispatch callback, eport_context argv)
{
  if (!callback) {
    return error::invalid_argument;
  }
  auto ios = find_handle(handle);
  if (ios) {
    ios->post([callback, argv]() { callback(argv); });
    return eport_error_ok;
  }
  return error::service_not_found;
}

/*******************************************************************************/

eport_errno eport_dispatch(eport_handle handle, eport_cb_dispatch callback, eport_context argv)
{
  if (!callback) {
    return error::invalid_argument;
  }
  auto ios = find_handle(handle);
  if (ios) {
    ios->dispatch([callback, argv]() { callback(argv); });
    return eport_error_ok;
  }
  return error::service_not_found;
}

/*******************************************************************************/

eport_errno eport_restart(eport_handle handle)
{
  auto ios = find_handle(handle);
  if (ios) {
    ios->restart();
    return eport_error_ok;
  }
  return error::service_not_found;
}

/*******************************************************************************/

eport_errno eport_stop(eport_handle handle)
{
  auto ios = find_handle(handle);
  if (ios) {
    ios->stop();
    return eport_error_ok;
  }
  return error::service_not_found;
}

/*******************************************************************************/

eport_errno eport_stopped(eport_handle handle)
{
  auto ios = find_handle(handle);
  if (ios) {
    return ios->stopped() ? eport_true : eport_false;
  }
  return error::service_not_found;
}

/*******************************************************************************/

eport_count eport_run(eport_handle handle, size_t expires)
{
  auto ios = find_handle(handle);
  if (!ios) {
    return 0;
  }
  if (expires == 0) {
    return ios->poll();
  }
  if (expires == (size_t)-1) {
    return ios->run();
  }
  return ios->run_for(
    std::chrono::milliseconds(expires)
  );
}

/*******************************************************************************/

eport_count eport_run_one(eport_handle handle, size_t expires)
{
  auto ios = find_handle(handle);
  if (!ios) {
    return 0;
  }
  if (expires == 0) {
    return ios->poll_one();
  }
  if (expires == (size_t)-1) {
    return ios->run_one();
  }
  return ios->run_one_for(
    std::chrono::milliseconds(expires)
  );
}

/*******************************************************************************/

eport_errno eport_resolve(const char* host, unsigned short port, eport_endpoint& endpoint)
{
  if (!host || !port) {
    return error::invalid_argument;
  }
  error_code ec;
  auto result = ip::socket::resolve(host, port, ec);
  if (!ec) {
    const ip::udp::endpoint& remote = *result.begin();
    copy_endpoint(remote, endpoint);
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_port(const eport_endpoint& endpoint, unsigned short& port)
{
  ip::udp::endpoint end;
  copy_endpoint(endpoint, end);
  port = end.port();
  return eport_error_ok;
}

/*******************************************************************************/

eport_errno eport_address(const eport_endpoint& endpoint, char* data, size_t size)
{
  if (!data || !size) {
    return error::invalid_argument;
  }
  ip::udp::endpoint end;
  copy_endpoint(endpoint, end);

  auto ip = end.address().to_string();
  if (ip.size() + 1 > size) {
    return error::no_memory;
  }
  memcpy(data, ip.c_str(), ip.size() + 1);
  return eport_error_ok;
}

/*******************************************************************************/

eport_handle eport_socket(eport_handle handle, eport_family family)
{
  switch (family) {
  case eport_family::stream:
  case eport_family::dgram:
    break;
  default:
    return eport_none;
  }
  auto ios = find_handle(handle);
  if (!ios) {
    return eport_none;
  }
  auto socket = ip::socket::create(ios, (ip::family)family);
  if (socket) {
    thread_lock_begin;
    eport_sockets[socket->id()] = socket;
  }
  return socket ? socket->id() : eport_none;
}

/*******************************************************************************/

eport_errno eport_close(eport_handle fd, size_t linger)
{
  auto socket = find_socket(fd);
  if (socket) {
    thread_lock_begin;
    if (eport_sockets.erase(socket->id())) {
      socket->close(linger ? true : false);
      return eport_error_ok;
    }
  }
  return eport_release(fd);
}

/*******************************************************************************/

eport_errno eport_ssl_enable(eport_handle fd)
{
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    auto ctx = ip::socket::context::create();
    if (!ctx) {
      ec = error::operation_not_supported;
    } else {
      ec.clear();
      socket->assign(ctx);
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_on_hostname(eport_handle fd, eport_cb_hostname callback, eport_context argv)
{
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    auto ctx = socket->ssl_context();
    if (!ctx) {
      ec = error::operation_not_supported;
    } else {
      ec.clear();
      ctx->use_sni_callback(callback, argv);
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_use_verify_file(eport_handle fd, const char* filename)
{
  if (!filename) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    auto ctx = socket->ssl_context();
    if (!ctx) {
      ec = error::operation_not_supported;
    } else {
      ec = ctx->load_verify_file(filename);
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_use_certificate(eport_handle fd, const char* cert, size_t size)
{
  if (!cert) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    auto ctx = socket->ssl_context();
    if (!ctx) {
      ec = error::operation_not_supported;
    } else {
      ec = ctx->use_certificate_chain(cert, size);
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_use_private_key(eport_handle fd, const char* key, size_t size, const char* pwd)
{
  if (!key) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    auto ctx = socket->ssl_context();
    if (!ctx) {
      ec = error::operation_not_supported;
    } else {
      ec = ctx->use_private_key(key, size, pwd);
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_listen(eport_handle fd, unsigned short port, const char* host, int backlog)
{
  error_code ec(error::bad_descriptor);
  auto acceptor = find_socket(fd);
  if (acceptor) {
    if (backlog < 5) {
      backlog = 5;
    }
    ec = acceptor->listen(port, host, backlog);
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_accept(eport_handle fd, eport_handle new_fd)
{
  error_code ec(
    error::bad_descriptor
  );
  auto acceptor = find_socket(fd);
  auto socket   = find_socket(new_fd);
  if (acceptor && socket) {
    ec = acceptor->accept(socket);
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_async_accept(eport_handle fd, eport_handle new_fd, eport_cb_accept callback, eport_context argv)
{
  if (!callback) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto acceptor = find_socket(fd);
  auto socket   = find_socket(new_fd);
  if (acceptor && socket) {
    ec.clear();
    acceptor->async_accept(socket,
      std::bind(
        [callback, argv](const error_code& ec, ip::socket::value acceptor, ip::socket::value socket) {
          callback(ec.value(), acceptor->id(), socket->id(), argv);
        },
        std::placeholders::_1,
        acceptor,
        socket
      )
    );
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_connect(eport_handle fd, const char* host, unsigned short port)
{
  if (!host || !port) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ec = socket->connect(host, port);
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_async_connect(eport_handle fd, const char* host, unsigned short port, eport_cb_connect callback, eport_context argv)
{
  if (!host || !port || !callback) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ec.clear();
    socket->async_connect(
      host, port, 
      std::bind(
        [callback, argv](const error_code& ec, ip::socket::value socket) {
          callback(ec.value(), socket->id(), argv);
        },
        std::placeholders::_1, socket
      )
    );
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_select(eport_handle fd, eport_event what, eport_cb_select callback, eport_context argv)
{
  if (!callback) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ec.clear();
    socket->async_select(
      ip::socket::wait_type(what),
      std::bind(
        [callback, argv](const error_code& ec, size_t bytes, ip::socket::value socket) {
          callback(ec.value(), socket->id(), bytes, argv);
        },
        std::placeholders::_1, std::placeholders::_2, socket
      )
    );
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_send(eport_handle fd, const char* data, size_t length, eport_count* count)
{
  if (!data) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    size_t n = socket->send(data, length, ec);
    if (count) {
      *count = n;
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_send_to(eport_handle fd, const char* data, size_t length, const eport_endpoint& remote, eport_count* count)
{
  if (!data) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ip::udp::endpoint peer;
    copy_endpoint(remote, peer);
    size_t n = socket->send_to(data, length, peer, ec);
    if (count) {
      *count = n;
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_async_send(eport_handle fd, const char* data, size_t length)
{
  if (!data) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ec.clear();
    socket->async_send(data, length);
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_async_send_to(eport_handle fd, const char* data, size_t length, const eport_endpoint& remote)
{
  if (!data) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ec.clear();
    ip::udp::endpoint peer;
    copy_endpoint(remote, peer);
    socket->async_send_to(data, length, peer);
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_receive(eport_handle fd, char* buffer, size_t size, eport_count* count)
{
  if (!buffer) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    size_t n = socket->receive(buffer, size, ec);
    if (count) {
      *count = n;
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_receive_from(eport_handle fd, char* buffer, size_t size, eport_endpoint& remote, eport_count* count)
{
  if (!buffer) {
    return error::invalid_argument;
  }
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    ip::socket::endpoint_type peer;
    size_t n = socket->receive_from(buffer, size, peer, ec);
    if (!ec) {
      copy_endpoint(peer, remote);
    }
    if (count) {
      *count = n;
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_remote_endpoint(eport_handle fd, eport_endpoint& remote)
{
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    if (socket->is_tcp_socket()) {
      ip::tcp::endpoint peer;
      ec = socket->remote_endpoint(peer);
      if (!ec) {
        copy_endpoint(peer, remote);
      }
    }
    if (socket->is_udp_socket()) {
      ip::udp::endpoint peer;
      ec = socket->remote_endpoint(peer);
      if (!ec) {
        copy_endpoint(peer, remote);
      }
    }
  }
  return ec.value();
}

/*******************************************************************************/

eport_errno eport_local_endpoint(eport_handle fd, eport_endpoint& local)
{
  error_code ec(
    error::bad_descriptor
  );
  auto socket = find_socket(fd);
  if (socket) {
    if (socket->is_tcp_socket()) {
      ip::tcp::endpoint peer;
      ec = socket->local_endpoint(peer);
      if (!ec) {
        copy_endpoint(peer, local);
      }
    }
    if (socket->is_udp_socket()) {
      ip::udp::endpoint peer;
      ec = socket->local_endpoint(peer);
      if (!ec) {
        copy_endpoint(peer, local);
      }
    }
  }
  return ec.value();
}

/*******************************************************************************/


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

#include <mutex>
#include <map>
#include <socket/socket.hpp>

#include "libsocket.io.h"

using namespace eth;

static const size_t conn_timeout = 300 * 1000;
static reactor_type ios = reactor::create();
static std::map<socket_fd, socket_type> sockets;

/*******************************************************************************/

lib_api socket_fd ltcp_create()
{
  socket_type socket = socket::create(ios, socket::family::sock_stream);
  if (!socket) {
    return 0;
  }
  socket_fd fd = socket->id();
  sockets[fd] = socket;
  return fd;
}

/*******************************************************************************/

lib_api int ltcp_update()
{
  return (int)ios->poll();
}

/*******************************************************************************/

lib_api void ltcp_close(socket_fd fd)
{
  auto iter = sockets.find(fd);
  if (iter == sockets.end()) {
    return;
  }
  socket_type socket = iter->second;
  socket->close(false);
  sockets.erase(iter);
}

/*******************************************************************************/

lib_api int ltcp_connect(socket_fd fd, const char* host, unsigned short port, connect_callback callback)
{
  if (!host || !port) {
    return error::invalid_argument;
  }
  auto iter = sockets.find(fd);
  if (iter == sockets.end()) {
    return error::bad_descriptor;
  }
  socket_type socket = iter->second;
  if (!callback)
  {
    error_code ec;
    ec = socket->connect(host, port, 5000);
    if (ec) {
      ltcp_close(fd);
    }
    else {
      socket->timeout(conn_timeout);
    }
    return ec.value();
  }
  socket->async_connect(host, port, [socket, fd, callback](const error_code& ec)
  {
    callback(ec.value(), fd);
    if (ec) {
      ltcp_close(fd);
    }
    else if (socket->is_open()) {
      socket->timeout(conn_timeout);
    }
  });
  return 0;
}

/*******************************************************************************/

lib_api int ltcp_send(socket_fd fd, const char* data, unsigned int size)
{
  if (!data || !size) {
    return error::invalid_argument;
  }
  auto iter = sockets.find(fd);
  if (iter == sockets.end()) {
    return error::bad_descriptor;
  }
  socket_type socket = iter->second;
  socket->encode(1, data, size, true, true, [socket](const char* data, size_t size) {
    error_code ec;
    socket->send(data, size, ec);
  });
  return (int)size;
}

/*******************************************************************************/

lib_api int ltcp_wait(socket_fd fd, receive_callback callback)
{
  auto iter = sockets.find(fd);
  if (iter == sockets.end()) {
    return error::bad_descriptor;
  }
  socket_type socket = iter->second;
  socket->async_wait(socket::wait_type::wait_read, [=](const error_code& ec, size_t bytes)
  {
    if (ec) {
      callback(ec.value(), 0, 0, fd);
      ltcp_close(fd);
      return;
    }
    int error = 0;
    bytes = socket->decode(bytes, error, [fd, callback](const char* data, size_t size, const decoder::header* header) {
      callback(0, data, (unsigned int)size, fd);
    });
    if (error > 0) {
      callback(error, 0, 0, fd);
      ltcp_close(fd);
      return;
    }
    if (!socket->is_open()) {
      callback(error::connection_reset, 0, 0, fd);
      return;
    }
  });
  return 0;
}

/*******************************************************************************/


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

#include <map>
#include "luaos_master.h"

static int thd_count = 1;
static int started   = 0;
static socket_type listener;
static io_handler  io_services[max_thd_count];
static std::shared_ptr<std::thread> io_threads[max_thd_count];
static std::map<int, socket_type> sessions;
static bool async_accept();
static void async_receive(socket_type peer);

/***********************************************************************************/

void send_to_peer(const std::string& data, socket_type peer)
{
  peer->encode(0, data.c_str(), data.size(), false, false,
    [&peer](const char* data, size_t size) {
      peer->async_send(data, size);
    }
  );
}

void send_to_all(const std::string& data)
{
  auto iter = sessions.begin();
  for (; iter != sessions.end(); iter++) {
    send_to_peer(data, iter->second);
  }
}

void send_to_other(const std::string& data, socket_type peer)
{
  auto iter = sessions.begin();
  for (; iter != sessions.end(); iter++) {
    if (iter->second->id() != peer->id()) {
      send_to_peer(data, iter->second);
    }
  }
}

/***********************************************************************************/

static void on_error(const error_code& ec, socket_type peer)
{
  sessions.erase(peer->id());
  peer->close(false);
}

static void on_request(const std::string& data, socket_type peer)
{

}

static void on_decode(const char* data, size_t size, const decoder::header*, socket_type peer)
{
  std::string packet(data, size);
  auto ios = listener->service();
  ios->post(std::bind(on_request, packet, peer));
}

static void on_receive(const error_code& ec, size_t bytes, socket_type peer)
{
  if (ec) {
    on_error(ec, peer);
    return;
  }
  int errcode = 0;
  size_t remainder = peer->decode(
    bytes, errcode, std::bind(
      on_decode, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, peer
    )
  );
  if (errcode) {
    peer->close(false);
    return;
  }
  async_receive(peer);
}

static void async_receive(socket_type peer)
{
  peer->async_wait(socket::wait_type::wait_read, std::bind(
    on_receive, std::placeholders::_1, std::placeholders::_2, peer)
  );
}

static void on_accept(const error_code& ec, socket_type peer)
{
  if (!listener->is_open()) {
    return;
  }
  async_accept();
  if (ec) {
    peer->close(false);
    return;
  }
  sessions[peer->id()] = peer;
  async_receive(peer);
}

static bool async_accept()
{
  io_handler ios = io_services[0];
  for (int i = 1; i < thd_count; i++) {
    if (io_services[i].use_count() < ios.use_count()) {
      ios = io_services[i];
    }
  }
  socket_type peer = socket::create(ios, socket::family::sock_stream);
  if (!peer) {
    return false;
  }
  listener->async_accept(peer, true, on_accept);
  return true;
}

/***********************************************************************************/

static void threadproc(int index)
{
  auto ios = luaos_local.lua_service();
  io_services[index] = ios;
  started++;
  while (!ios->stopped()) { ios->run(); }
}

void luaos_stop_master()
{
  if (listener) {
    listener->close();
  }
  for (int i = 0; i < thd_count; i++) {
    io_services[i]->stop();
    if (io_threads[i]->joinable()) {
      io_threads[i]->join();
      io_threads[i].reset();
    }
  }
}

bool luaos_start_master(const char* host, unsigned short port, int threads)
{
  if (listener) {
    return false;
  }
  thd_count = threads;
  if (thd_count < 1) {
    thd_count = 1;
  }
  if (thd_count > 4) {
    thd_count = 4;
  }
  auto ios = luaos_local.lua_service();
  for (int i = 0; i < thd_count; i++) {
    io_threads[i].reset(
      new std::thread(std::bind(threadproc, i))
    );
  }
  while (started != thd_count) {
    std::this_thread::sleep_for(
      std::chrono::milliseconds(100)
    );
  }
  listener = socket::create(ios, socket::family::sock_stream);
  if (!listener) {
    luaos_stop_master();
    return false;
  }
  error_code ec = listener->listen(port, host, 64);
  if (ec) {
    luaos_stop_master();
    return false;
  }
  return async_accept();
}

/***********************************************************************************/

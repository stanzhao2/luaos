

--[[
*********************************************************************************
** 
** Copyright 2021-2022 LuaOS
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
*********************************************************************************
]]--

local luaos  = require("luaos");
local socket = luaos.socket;
local bind   = luaos.bind;
local pack   = luaos.conv.pack;

----------------------------------------------------------------------------

local cmd_subscribe  = "subscribe"
local cmd_cancel     = "cancel"
local cmd_ready      = "ready"
local cmd_publish    = "publish"
local cmd_heartbeat  = "heartbeat"

local _MAX_PACKET    = 64 * 1024 * 1024

----------------------------------------------------------------------------

local function lshift(v, n)
	return v << n;
end

local function rshift(v, n)
	return v >> n;
end

local function bitand(v, n)
	return v & n;
end

----------------------------------------------------------------------------

local server = { peer = nil, ready = false };

----------------------------------------------------------------------------

local function send_to_peer(peer, data)
	peer:send(peer:encode(data));
end

local function send_to_master(message)
	if server.peer then
		send_to_peer(server.peer, pack.encode(message));
	end
end

local function send_keepalive(now)
	local message = {};
	message.type = cmd_heartbeat;
	server.keepalive = now;
	
	send_to_master(message);
end

local function on_publish_request(topic, publisher, mask, ...)
	local message     = {}
	message.type      = cmd_publish
	message.topic     = topic
	message.argv      = {...};
	message.mask      = mask
	message.publisher = publisher
	
	send_to_master(message)
end

local function on_subscribe_request(topic, subscriber, option)
	local message = {};
	message.topic = topic;
	message.subscriber = subscriber;
	
	if option == "cancel" then
		message.type = cmd_cancel;
	else
		message.type = cmd_subscribe;
	end
	
	send_to_master(message);
end

----------------------------------------------------------------------------

local function do_ready_request(message)
	server.ready = true;
	print("server is ready of cluster");
end

local function do_publish_request(message)
	local publisher = message.publisher;
	local receiver  = rshift(publisher, 32) -- >>32bits
	
	if receiver > 0 then
		publisher = receiver;
	else
		publisher = lshift(publisher, 16); -- <<16bits
	end
	
	local params = message.argv;
	luaos.publish(message.topic, message.mask, publisher, table.unpack(params));
end

local function do_cancel_request(message)
	luaos.cancel(message.topic);
end

local function do_subscribe_request(message)
	luaos.subscribe(message.topic, bind(on_publish_request, message.topic));
end

----------------------------------------------------------------------------

local switch = {
	[cmd_publish]   = do_publish_request,
	[cmd_cancel]    = do_cancel_request,
	[cmd_ready]     = do_ready_request,
	[cmd_subscribe] = do_subscribe_request,
}

local function on_socket_error(ec)
	error("The connection with server has been disconnected");
	server.peer:close();
	server.peer = nil;
end

local function on_socket_dispatch(peer, data)
	local message = pack.decode(data);
	if type(message) == "table" then
		luaos.pcall(switch[message.type], message);
	end
end

local function on_socket_receive(peer, ec, data)	
	if ec > 0 then
		on_socket_error(ec);
		return;
	end
	
	local size = peer:decode(data, function(data, opcode)
		if not luaos.pcall(on_socket_dispatch, peer, data) then
			peer:close();
		end
	end);
	
	if not size or size > _MAX_PACKET then
		peer:close();
	end	
end

----------------------------------------------------------------------------

local proxy = {};

function proxy.watch(topic)
	luaos.watch(topic, bind(on_subscribe_request, topic));
end

function server.stop()
	if server.peer then
		server.peer:close();
		server.peer = nil;
	end
end

function proxy.update(now)
	if proxy.keepalive == nil then
		proxy.keepalive = now
		return
	end
	
	if now - proxy.keepalive < 10000 then
		return
	end
	
	send_keepalive(now)
end

function proxy.start(host, port, timeout)
	if server.peer then
		return false;
	end
	
	host = host or "0.0.0.0";
	port = port or 8899;
	timeout = timeout or 2000;
    
	local peer = luaos.socket("tcp");
	if not peer then
		return false;
	end
	
	local ok, reason = peer:connect(host, port, timeout);
	if ok then
		peer:select(luaos.read, bind(on_socket_receive, peer));
		server.peer = peer;
	end
	return ok, reason;
end

----------------------------------------------------------------------------

return proxy

----------------------------------------------------------------------------

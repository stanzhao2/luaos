

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
local insert = table.insert;
local remove = table.remove;

----------------------------------------------------------------------------

local cmd_subscribe  = "subscribe";
local cmd_cancel     = "cancel";
local cmd_ready      = "ready";
local cmd_publish    = "publish";
local cmd_heartbeat  = "heartbeat";

local _MAX_PACKET    = 64 * 1024 * 1024

----------------------------------------------------------------------------

local last_onlines, last_performance = 0, 0;
local sessions, onlines, performance = {}, 0, 0;
--[=[
sessions = {fd={topic={subscriber,...}, peer}, ...}
--]=]

----------------------------------------------------------------------------

local function send_to_peer(peer, data)
	peer:send(peer:encode(data), true);
end

---Forward to all sessions except fd
local function send_to_others(session, tb)
	local peer    = session.peer;
	local fd      = peer:id();
	local message = pack.encode(tb);
	
	for k, v in pairs(sessions) do
		if k ~= fd then
			send_to_peer(v.peer, message);
		end
	end
end

---Determine whether the session has subscribed to a topic
local function is_subscribed(session, topic)
	for k, _ in pairs(session) do
		if k == topic then
			return true;
		end
	end
	return false;
end

---Forward to all sessions subscribed to this topic except fd
local function on_publish_request(session, tb)
	local target  = {};
	local peer    = session.peer;
	local fd      = peer:id();
	local message = pack.encode(tb);
	
	local topic = tb.topic;
	for k, v in pairs(sessions) do
		if k ~= fd then
			if is_subscribed(v, topic) then
				insert(target, v);
			end
		end
	end
	
	local count = #target;
	
	--only send to one session
	if tb.mask > 0 and count > 1 then
		local i = (tb.mask % count) + 1;
		send_to_peer(target[i].peer, message);
		return
	end
	
	--broadcast to all subscribers
	for i = 1, count do
		send_to_peer(target[i].peer, message);
	end
end

---Unsubscribe from a topic
local function on_cancel_request(session, tb)
	local topic      = tb.topic;
	local subscriber = tb.subscriber;

	if session[topic] == nil then
		return;
	end
	
	send_to_others(session, tb);
	
	local count = #session[topic];
	for i = 1, count do
		if session[topic][i] == subscriber then
			remove(session[topic], i);
			break
		end
	end
	
	if #session[topic] == 0 then
		session[topic] = nil;
	end
	print("cancel topic: " .. topic);
end

---Subscribe to messages on a topic
local function on_subscribe_request(session, tb)
	local topic      = tb.topic;
	local subscriber = tb.subscriber;
	
	if session[topic] == nil then
		session[topic] = {};
	end
	
	for i = 1, #session[topic] do
		if session[topic][i] == subscriber then
			return;
		end
	end
	
	insert(session[topic], subscriber);
	send_to_others(session, tb);
	print("subscribe topic: " .. topic);
end

---Called when the session has an error
local function on_socket_error(session, ec)
	local peer = session.peer;
	local fd   = peer:id();
	
	if sessions[fd] then
		sessions[fd] = nil;
		onlines = onlines - 1;
	end
	
	peer:close();
	error("session error, errno " .. ec);
end

local function on_socket_dispatch(session, data)	
	local peer = session.peer;
	local tb = pack.decode(data);
	if type(tb) ~= "table" then
		peer:close();
		return;
	end
	
	local cmd = tb.type;
	if cmd == cmd_heartbeat then
		return;
	end
	
	if cmd == cmd_subscribe then
		on_subscribe_request(session, tb);
		return;
	end
	
	if cmd == cmd_cancel then
		on_cancel_request(session, tb);
		return;
	end
	
	if cmd ~= cmd_publish then
		peer:close();
		return;
	end
	
	performance = performance + 1;
	local publisher = tb.publisher;
	local high = publisher >> 16; -- >>16bits
	
	local fd = peer:id();	
	if high == 0 then
		publisher = publisher << 16; -- <<16bits
		tb.publisher = publisher + fd;
		on_publish_request(session, tb);
	else
		session = sessions[high & 0xffff];
		if session then
			tb.publisher = (publisher & 0xFFFF0000FFFF) | (fd << 16)
			data = pack.encode(tb);
			send_to_peer(session.peer, data);
		end
	end
end

---Called when data is received from a session
local function on_socket_receive(session, ec, data)
	if ec > 0 then
		on_socket_error(session, ec);
		return;
	end
	
	local peer = session.peer;
	local size = peer:decode(data, function(data, opcode)
		if not luaos.pcall(on_socket_dispatch, session, data) then
			peer:close();
		end
	end);
	
	if not size or size > _MAX_PACKET then
		peer:close();
	end
end

---Called when a new session is established
local function on_socket_accept(acceptor, peer)
	local fd   = peer:id();
	local from = peer:endpoint();
	local tb, subscribed = {}, {};
	
	tb.type = cmd_subscribe;
	for k, v in pairs(sessions) do
		for topic, x in pairs(v) do
			if type(x) == "table" then
				if subscribed[topic] == nil then
					tb.topic = topic;
					send_to_peer(peer, pack.encode(tb));
					subscribed[topic] = true;
				end
			end
		end
	end
	
	sessions[fd] = {peer = peer};
	onlines = onlines + 1;
	
	tb.type = cmd_ready;	
	send_to_peer(peer, pack.encode(tb));
	
	peer:select(luaos.read, bind(on_socket_receive, sessions[fd]));	
	print("new session create from " .. from);
end

----------------------------------------------------------------------------

local master = {};

----------------------------------------------------------------------------

function master:update()
	local now = os.clock();
	
	if master.keepalive == nil then
		master.keepalive = now;
		return
	end
	
	if now - master.keepalive < 1 then
		return
	end
	
	if onlines ~= last_onlines or performance ~= last_performance then
		last_onlines = onlines;
		last_performance = performance;
		print(string.format("Number of sessions: %d, Forwarding quantity: %d", onlines, performance));
	end
	
	performance = 0;
	master.keepalive = now;
end

function master.stop()
	if master.acceptor then
		master.acceptor:close();
		master.acceptor = nil;
	end
	
	for k, v in pairs(sessions) do
		v.peer:close();
		v.peer = nil;
	end
	sessions = {};
end

function master.start(host, port)
	if master.acceptor then
		return false;
	end
	
	host = host or "0.0.0.0";
	port = port or 8899;
    
	local acceptor = luaos.socket("tcp");
	if not acceptor then
		return false;
	end
	
	local ok, reason = acceptor:listen(
		host, port, bind(on_socket_accept, acceptor)
	);
	if ok then
		master.acceptor = acceptor;
	end
	return ok, reason;
end

----------------------------------------------------------------------------

return master;

----------------------------------------------------------------------------

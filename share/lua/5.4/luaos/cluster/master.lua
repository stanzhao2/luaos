﻿

--[[
*********************************************************************************
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
*********************************************************************************
]]--

local luaos  = require("luaos");
local socket = luaos.socket;
local bind   = luaos.bind;
local pack   = luaos.conv.pack;
local insert = table.insert;
local remove = table.remove;
local format = string.format;

local pcall = pcall;
if _DEBUG then
    pcall = require("luaos.pcall");
end

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
    for k, v in pairs(session) do
        if k == topic and v == true then
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
    
    if _DEBUG then
        local peer = session.peer;
        local fd = peer:id();
        print(format("session %d publish topic %d", fd, topic));
    end
    
    local count = #target;
    
    --only send to one session
    if tb.mask > 0 and count > 1 then
        local i = (session.mask % count) + 1;
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
    local topic = tb.topic;
    if session[topic] then
        send_to_others(session, tb);
        session[topic] = nil;
    end
    
    if _DEBUG then
        local peer = session.peer;
        local fd = peer:id();
        print(format("session %d cancel topic %d", fd, topic));
    end
end

---Subscribe to messages on a topic
local function on_subscribe_request(session, tb)
    local topic = tb.topic;
    session[topic] = true;
    send_to_others(session, tb);
    
    if _DEBUG then
        local peer = session.peer;
        local fd = peer:id();
        print(format("session %d subscribe topic %d", fd, topic));
    end
end

---Called when the session has an error
local function on_socket_error(session, ec, msg)
    local peer = session.peer;
    local fd   = peer:id();
    
    if sessions[fd] then
        sessions[fd] = nil;
        onlines = onlines - 1;
    end
    
    for topic, v in pairs(session) do
        if v == true then
            local message = {};
            message.type  = cmd_cancel;
            message.topic = topic;
            send_to_others(session, message);
        end
    end
    
    peer:close();
    if _DEBUG then
        print(format("session %d error: %s", fd, msg));
    end
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
        send_to_peer(session.peer, data);
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
        on_socket_error(session, ec, data);
        return;
    end
    
    local peer = session.peer;
    local size, reason = peer:decode(data, function(data, opcode)
        if not pcall(on_socket_dispatch, session, data) then
            peer:close();
        end
    end);
    
    if not size then
        peer:close();
        error("packet decode error: ", reason);
        return;
    end
    
    if size > _MAX_PACKET then
        peer:close();
        error("packet length too large: ", size);
    end
end

---Called when a new session is established
local function on_socket_accept(acceptor, peer)
    local fd = peer:id();
    local from, port = peer:endpoint();
    local tb = {};
    
    tb.type = cmd_subscribe;
    for k, v in pairs(sessions) do
        for topic, x in pairs(v) do
            if x == true then
                tb.topic = topic;
                send_to_peer(peer, pack.encode(tb));
            end
        end
    end
    
    sessions[fd] = {
        peer = peer,
        mask = math.random(0, 0xffffffff)
    };
    onlines = onlines + 1;
    
    tb.type = cmd_ready;    
    send_to_peer(peer, pack.encode(tb));    
    peer:select(luaos.read, bind(on_socket_receive, sessions[fd])); 
    
    if _DEBUG then
        print(format("session %d accept: %s:%d", fd, from, port));
    end
end

----------------------------------------------------------------------------

local master, timer = {}, nil;

----------------------------------------------------------------------------

local function update_master(interval)
    if onlines ~= last_onlines or performance ~= last_performance then
        last_onlines = onlines;
        last_performance = performance;
        print(format("Number of sessions: %d, Forwarding quantity: %d", onlines, performance));
    end 
    performance = 0;
    timer = luaos.scheme(interval, bind(update_master, interval));
end

function master.stop()
    if master.acceptor then
        master.acceptor:close();
        master.acceptor = nil;
    end
    if timer then
        timer:cancel();
    end
    for k, v in pairs(sessions) do
        v.peer:close();
        v.peer = nil;
    end
    sessions = {};
end

function master.start(host, port)
    assert(host and port);
    if master.acceptor then
        return false;
    end
    
    local acceptor = socket("tcp");
    if not acceptor then
        return false;
    end
    
    local ok, reason = acceptor:listen(
        host, port, bind(on_socket_accept, acceptor)
    );
    if ok then
        master.acceptor = acceptor;
        timer = luaos.scheme(1000, bind(update_master, 1000));
    end
    return ok, reason;
end

----------------------------------------------------------------------------

return master;

----------------------------------------------------------------------------

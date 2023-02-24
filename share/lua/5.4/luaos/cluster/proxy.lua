

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
local unpack = table.unpack;
local format = string.format;

local pcall = pcall;
if _DEBUG then
    pcall = require("luaos.pcall");
end

----------------------------------------------------------------------------

local cmd_subscribe   = "subscribe"
local cmd_cancel      = "cancel"
local cmd_ready       = "ready"
local cmd_publish     = "publish"
local cmd_heartbeat   = "heartbeat"
 
local _MAX_PACKET     = 64 * 1024 * 1024

----------------------------------------------------------------------------

local local_topics    = {};
local remote_topics   = {};
local server = { peer = nil, ready = false };

----------------------------------------------------------------------------

local function send_to_peer(peer, data)
    peer:send(peer:encode(data), true);
end

local function send_to_master(message)
    if server.peer then
        send_to_peer(server.peer, pack.encode(message));
    end
end

local function luaos_publish(topic, mask, receiver, ...)
    luaos.publish(topic, mask, receiver, ...);
end

local function luaos_cancel(topic, reason)
    luaos.cancel(topic);
    trace(format("topic %d is cancelled by %s proxy", topic, reason));
end

local function luaos_subscribe(topic, handler, reason)
    luaos.subscribe(topic, handler);
    trace(format("topic %d is subscribed by %s proxy", topic, reason));
end

----------------------------------------------------------------------------

local function on_local_publish(topic, publisher, mask, ...)
    --如果没有其他节点订阅该主题
    if not remote_topics[topic] then
        return;
    end
    
    local message     = {}
    message.type      = cmd_publish
    message.topic     = topic
    message.argv      = {...};
    message.mask      = mask
    message.publisher = publisher
    
    send_to_master(message)
end

local function on_local_cancel(topic, subscriber)  
    if not local_topics[topic] then
        return;
    end
    
    local_topics[topic] = local_topics[topic] - 1;
    if local_topics[topic] > 0 then
        return;
    end
    
    local_topics[topic] = nil;

    local message = {};
    message.type  = cmd_cancel;
    message.topic = topic;
    
    send_to_master(message);
    
    --如果没有其他节点订阅该主题
    --if not remote_topics[topic] then
    --    luaos_cancel(topic, "local");
    --end
end

local function on_local_subscribe(topic, subscriber)    
    if local_topics[topic] then
        local_topics[topic] = local_topics[topic] + 1;
        return;
    end
    
    local_topics[topic] = 1;
    
    local message = {};
    message.type  = cmd_subscribe;
    message.topic = topic;
    
    send_to_master(message);
    
    --如果没有其他节点订阅该主题
    --if not remote_topics[topic] then
    --    luaos_subscribe(topic, bind(on_local_publish, topic), "local");
    --end
end

local function on_local_watch(topic, subscriber, option)
    if option == "cancel" then
        on_local_cancel(topic, subscriber);
        return;
    end
    
    if option == "subscribe" then
        on_local_subscribe(topic, subscriber);
        return;
    end
end

----------------------------------------------------------------------------

local function on_remote_ready(message)
    server.ready = true;
    print("cluster is ready");
end

local function on_remote_publish(message)
    local publisher = message.publisher;
    local receiver  = publisher >> 32; -- >>32bits
    
    if receiver > 0 then
        publisher = ((publisher & 0xFFFF) << 32) | (publisher & 0xFFFF0000) | receiver;
    else
        publisher = publisher << 16; -- <<16bits
    end
    
    local params = message.argv;
    luaos_publish(message.topic, message.mask, publisher, unpack(params));
end

local function on_remote_cancel(message)
    local topic = message.topic;
    if not remote_topics[topic] then
        return;
    end
    
    remote_topics[topic] = remote_topics[topic] - 1;
    if remote_topics[topic] > 0 then
        return;
    end
    
    remote_topics[topic] = nil;
    luaos_cancel(topic, "remote");
end

local function on_remote_subscribe(message)
    local topic = message.topic;
    if remote_topics[topic] then
        remote_topics[topic] = remote_topics[topic] + 1;
        return;
    end
    
    remote_topics[topic] = 1;
    luaos_subscribe(topic, bind(on_local_publish, topic), "remote");
end

----------------------------------------------------------------------------

local switch = {
    [cmd_heartbeat] = function() end,
    [cmd_publish]   = on_remote_publish,
    [cmd_cancel]    = on_remote_cancel,
    [cmd_ready]     = on_remote_ready,
    [cmd_subscribe] = on_remote_subscribe,
}

local function on_socket_error(ec)
    error("The connection with server has been disconnected");
    server.peer:close();
    server.peer = nil;
end

local function on_socket_dispatch(peer, data)
    local message = pack.decode(data);
    if type(message) == "table" then
        pcall(switch[message.type], message);
    end
end

local function on_socket_receive(peer, ec, data)    
    if ec > 0 then
        on_socket_error(ec);
        return;
    end
    
    local size, reason = peer:decode(data, function(data, opcode)
        if not pcall(on_socket_dispatch, peer, data) then
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

----------------------------------------------------------------------------

local proxy, timer = {}, nil;

local function update_proxy(interval)
    local message = {};
    message.type = cmd_heartbeat;   
    send_to_master(message);
    
    timer = luaos.scheme(interval, bind(update_proxy, interval));
end

function proxy.watch(topic)
    luaos.watch(topic, bind(on_local_watch, topic));
end

function proxy.stop()
    if server.peer then
        if timer then
            timer:cancel();
        end
        server.peer:close();
        server.peer = nil;
    end
end

function proxy.start(host, port, timeout)
    if server.peer then
        return false;
    end
    
    host = host or "0.0.0.0";
    port = port or 7621;
    timeout = timeout or 2000;
    
    local peer = socket("tcp");
    if not peer then
        return false;
    end
    
    local ok, reason = peer:connect(host, port, timeout);
    if ok then
        server.peer = peer;
        peer:select(luaos.read, bind(on_socket_receive, peer));
        timer = luaos.scheme(30000, bind(update_proxy, 30000));
    end
    return ok, reason;
end

----------------------------------------------------------------------------

return proxy

----------------------------------------------------------------------------

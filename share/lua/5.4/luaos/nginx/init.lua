

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

local luaos    = require("luaos");
local hash     = luaos.conv.hash;
local format   = string.format;
local sessions = {};
local event_base = hash.crc32("Websocket.v13");

local nginx = {
    event = {
        accept  = event_base + 1,
        receive = event_base + 2,
        send    = event_base + 3,
        close   = event_base + 4,
        error   = event_base + 5,
    }
};

local function ws_publish(topic, receiver, fd, ...)
    luaos.publish(topic, 0, receiver, fd, ...);
end

local function on_ws_accept(ws_peer, request, params)
    local from, port = ws_peer:endpoint();
    local fd = ws_peer:id();
    sessions[fd] = {
        from = from,
        port = port,
        peer = ws_peer,
    };
    local url     = request:url();
    local headers = request:headers();
    local pid     = luaos.pid();
    ws_publish(nginx.event.accept, pid, fd, headers, from, port);
end

local function on_ws_error(ws_peer, ec)
    local fd = ws_peer:id();
    sessions[fd] = nil;
    local pid = luaos.pid();
    ws_publish(nginx.event.error, pid, fd, ec);
end

local function on_ws_request(ws_peer, ec, data, opcode)
    if ec > 0 then
        on_ws_error(ws_peer, ec);
        return;
    end
    local fd = ws_peer:id();
    local session = sessions[fd];
    if session then
        local pid = luaos.pid();
        ws_publish(nginx.event.receive, pid, fd, data, opcode);
    end
end

local function on_ws_close(fd)
    local session = sessions[fd];
    if session then
        session.peer:close();
    end
end

local function on_ws_send(fd, data, opcode)
    local session = sessions[fd];
    if session then
        session.peer:send(data, opcode);
    end
end

----------------------------------------------------------------------------

local switch = {
    [nginx.event.accept]  = function(callback, publisher, mask, ...)
        local ok, err = pcall(callback, ...);
        if not ok then
            error(err);
        end
    end,
    [nginx.event.error]   = function(callback, publisher, mask, ...)
        local ok, err = pcall(callback, ...);
        if not ok then
            error(err);
        end
    end,
    [nginx.event.receive] = function(callback, publisher, mask, ...)
        local ok, err = pcall(callback, ...);
        if not ok then
            error(err);
        end
    end,
};

function nginx.listen(host, port, certs)
    if nginx.server then
        return;
    end
    local server, err = luaos.start("luaos.nginx.service", host, port, certs);
    if not server then
        throw(err);
    end
    nginx.server = server;
    nginx.childid = server:id();
    return nginx;
end

function nginx.close(fd)
    if fd then
        ws_publish(nginx.event.close, nginx.childid, fd);
        return;
    end
    if not nginx.server then
        return;
    end
    for k, v in pairs(switch) do
        luaos.cancel(k);
    end
    luaos.cancel(
        nginx.event.close
    );
    luaos.cancel(
        nginx.event.send
    );
    nginx.server:stop();
    nginx.server = nil;
end

function nginx.send(fd, data, opcode)
    ws_publish(nginx.event.send, nginx.childid, fd, data, opcode);
end

function nginx.on(event, callback)
    luaos.subscribe(event, luaos.bind(switch[event], callback));
    return nginx;
end

----------------------------------------------------------------------------
--以下代码为标准接口, 不要修改

local function initialize()
    luaos.subscribe(nginx.event.close, function(publisher, mask, ...)
        on_ws_close(...);
    end);        
    luaos.subscribe(nginx.event.send, function(publisher, mask, ...)
        on_ws_send(...);
    end);
end

---Internal function, do not call
function nginx.on_handshake(ws_peer, request, params)
    if initialize then
        initialize = initialize();
    end
    return true;
end

---Internal function, do not call
function nginx.on_accept(ws_peer, request, params)
	on_ws_accept(ws_peer, request, params);
end

---Internal function, do not call
function nginx.on_receive(ws_peer, ec, data, opcode)
	on_ws_request(ws_peer, ec, data, opcode);
end

----------------------------------------------------------------------------

return nginx;

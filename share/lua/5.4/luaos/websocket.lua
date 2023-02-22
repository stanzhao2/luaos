

local luaos    = require("luaos");
local hash     = luaos.conv.hash;
local format   = string.format;
local sessions = {};
local filename = {...};
local event_base = hash.crc32("Websocket 3.0") << 31;

local websocket = {
    event = {
        accept  = event_base + 1,
        receive = event_base + 2,
        send    = event_base + 3,
        close   = event_base + 4,
        error   = event_base + 5,
    }
};

--Create a TLS context for the specified hostname
local function tls_context(hostname)
    local hostcert = certificates[hostname];
    if not hostcert then
        return nil;
    end
    local context = tls.context();
    assert(context:load(hostcert.cert, hostcert.key, hostcert.passwd));
    return context;
end

local function ws_publish(topic, fd, ...)
    luaos.publish(topic, 0, 0, fd, ...);
end

local function on_ws_accept(ws_peer, request, params)
    local from, port = ws_peer:endpoint();
    local fd = ws_peer:id();
    sessions[fd] = {
        from = from,
        port = port,
        peer = ws_peer,
    };
    local url = request:url();
    ws_publish(websocket.event.accept, fd, from, port);
end

local function on_ws_error(ws_peer, ec)
    local fd = ws_peer:id();
    sessions[fd] = nil;
    ws_publish(websocket.event.error, fd, ec);
end

local function on_ws_request(ws_peer, ec, data, opcode)
    if ec > 0 then
        on_ws_error(ws_peer, ec);
        return;
    end
    local fd = ws_peer:id();
    local session = sessions[fd];
    if session then
        ws_publish(websocket.event.receive, fd, data, opcode);
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
    [websocket.event.accept]  = function(callback, publisher, mask, ...)
        callback(...);
    end,
    [websocket.event.error]   = function(callback, publisher, mask, ...)
        callback(...);
    end,
    [websocket.event.receive] = function(callback, publisher, mask, ...)
        callback(...);
    end,
};

function websocket.listen(host, port, certs)
    if websocket.server then
        return;
    end
    local server, err = luaos.start(filename[1], host, port, certs);
    if not server then
        throw(err);
    end
    websocket.server = server;
    return websocket;
end

function websocket.send(fd, data, opcode)
    ws_publish(websocket.event.send, fd, data, opcode);
end

function websocket.close(fd)
    if fd then
        ws_publish(websocket.event.close, fd);
        return;
    end
    if websocket.server then
        websocket.server:stop();
        websocket.server = nil;
    end
end

function websocket.on(event, callback)
    luaos.subscribe(event, luaos.bind(switch[event], callback));
    return websocket;
end

----------------------------------------------------------------------------
--以下代码为标准接口, 不要修改

function main(host, port, certs)
    if not certs then
        tls_context = nil;
    else
        certificates = certs;
    end
    
	local nginx, result = luaos.nginx.start(host, port, "wwwroot", tls_context);
    assert(nginx, result);    
    nginx:upgrade(); --websocket enabled

	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end	
    
	nginx:stop();
end

local function initialize()
    luaos.subscribe(websocket.event.close, function(publisher, mask, ...)
        on_ws_close(...);
    end);        
    luaos.subscribe(websocket.event.send, function(publisher, mask, ...)
        on_ws_send(...);
    end);
end

---Internal function, do not call
function websocket.on_handshake(ws_peer, request, params)
    if initialize then
        initialize = initialize();
    end
    return true;
end

---Internal function, do not call
function websocket.on_accept(ws_peer, request, params)
	on_ws_accept(ws_peer, request, params);
end

---Internal function, do not call
function websocket.on_receive(ws_peer, ec, data, opcode)
	on_ws_request(ws_peer, ec, data, opcode);
end

----------------------------------------------------------------------------

return websocket;

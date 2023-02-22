

local luaos     = require("luaos");
local ws_socket = require("luaos.websocket");
local ws_event  = ws_socket.event;
local format    = string.format;

local function on_receive(fd, data, opcode)
    ws_socket.send(fd, data, opcode);
end

local function on_error(fd, ec)
    trace(format("Websocket connection error, id=%d code=%d", fd, ec));
end

local function on_accept(fd, from, port)
    trace(format("Websocket accepted, id=%d from %s:%d", fd, from, port));
end

function main(...)
    ws_socket.on(ws_event.accept, on_accept)
        .on(ws_event.error, on_error)
        .on(ws_event.receive, on_receive)
        .listen("0.0.0.0", 8899)

	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end	
    
	ws_socket.close();
end

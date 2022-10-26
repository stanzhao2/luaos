

local luaos = require("luaos")

local function on_ws_receive(peer, ec, data, opcode, deflate)
	if ec > 0 then
		trace("websocket error: ", ec);
		return;
	end
	peer:send(data, opcode, deflate);
end

function main()
	local nginx = require("luaos.nginx")
	nginx.upgrade(on_ws_receive);
	local ok, reason = nginx.start("0.0.0.0", 8899, "wwwroot")
	if ok then
		while not luaos.stopped() do
			luaos.wait()
		end
		nginx.stop()
	else
		error(reason);
	end
end

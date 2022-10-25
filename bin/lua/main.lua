

local luaos = require("luaos")

local function on_ws_receive(peer, ec, data, op, deflate)
	if ec > 0 then
		trace("websocket error: ", ec);
		return;
	end
	peer:send(data, op, deflate);
end

function main()
	local nginx = require("luaos.nginx")
	nginx.upgrade(on_ws_receive);
	local success = nginx.start("0.0.0.0", 8899, "wwwroot")
	if success then
		while not luaos.stopped() do
			luaos.wait()
		end
		nginx.stop()
	end
end

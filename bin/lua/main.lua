

local luaos = require("luaos")

function main()
	local nginx = luaos.nginx;
	local server, reason = nginx.start("0.0.0.0", 8899, "wwwroot")
	
	if not server then
		error(reason);
		return;
	end
	
	server:upgrade(); --websocket enabled
	
	while not luaos.stopped() do
		luaos.wait()
	end
	
	server:stop()
end

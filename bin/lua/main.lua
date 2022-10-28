

local luaos = require("luaos")

function main()
	local nginx = require("luaos.nginx")
	local ok, reason = nginx.start("0.0.0.0", 8899, "wwwroot")
	if ok then
		nginx.upgrade();
		while not luaos.stopped() do
			luaos.wait()
		end
		nginx.stop()
	else
		error(reason);
	end
end

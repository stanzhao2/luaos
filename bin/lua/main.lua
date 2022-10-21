

local luaos = require("luaos")
local nginx = require("luaos.nginx")

function main()
	local success = nginx.run("wwwroot", 8899)
	if success then
		while not luaos.stopped() do
			luaos.wait()
		end
		nginx.stop()
	end
end

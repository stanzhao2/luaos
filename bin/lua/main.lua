

local luaos = require("luaos")
local nginx = require("luaos.nginx")

local sslctx = luaos.ssl.context("iccgame.com.crt", "iccgame.com.key");

function main()
	local success = nginx.run("wwwroot", 8899, "0.0.0.0", sslctx)
	if success then
		while not luaos.stopped() do
			luaos.wait()
		end
		nginx.stop()
	end
end

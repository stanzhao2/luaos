

local luaos = require("luaos");

function main(port)
    local master, result = luaos.cluster.listen("0.0.0.0", port);
	if not master then
		throw(result);
	end
    
	while not luaos.stopped() do
		local success, result = pcall(luaos.wait);
        if not success then
            error(result);
        end
	end	
    
    master:close();
end

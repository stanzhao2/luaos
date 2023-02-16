
local luaos = require("luaos");

function main(...)
	local nginx, result = luaos.nginx.start(
        "0.0.0.0", 8899, "wwwroot"
    );
	if not nginx then
		throw(result);
	end	
    
	nginx:upgrade(); --websocket enabled    
    local master = luaos.start("master", 7890);
    
	while not luaos.stopped() do
		local success, result = pcall(luaos.wait);
        if not success then
            error(result);
        end
	end	
    
    if master then
        master:stop();
    end
	nginx:stop();
end

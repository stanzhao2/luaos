
local luaos = require("luaos");

local https = {
    cert     = nil,  --Certificate file name
    key      = nil,  --Certificate key file
    password = nil,  --Certificate key file password
}

local function ssl_context()
    local context = nil;    
    if https.cert then
        context = luaos.ssl.context(https.cert, https.key, https.password);
        assert(context, "certificate loading failed");
    end
    return context;
end

function main(...)
    local master, result = luaos.start("master", 7890);
    assert(master, result);
    
	local nginx, result = luaos.nginx.start("0.0.0.0", 8899, "wwwroot", ssl_context());
    assert(nginx, result);
    nginx:upgrade(); --websocket enabled
      
	while not luaos.stopped() do
		local success, result = pcall(luaos.wait);
        if not success then
            error(result);
        end
	end	
    
    master:stop();
	nginx:stop();
end


local luaos = require("luaos");

--Certificate corresponding to host name
local certificates = {
    ["www.luaos.net"] = {
        cert = "luaos.net.crt",
        key  = "luaos.net.key",
    }
}

--Create a TLS context for the specified hostname
local function tls_context(hostname)
    local hostcert = certificates[hostname];
    if not hostcert then
        return nil;
    end
    local context = tls.context();
    context:load(hostcert.cert, hostcert.key, hostcert.passwd);
    return context;
end

function main(...)
    local master, result = luaos.start("master", 7890);
    assert(master, result);
    
    local proto_https = false;
	local nginx, result = luaos.nginx.start("0.0.0.0", 8899, "wwwroot");
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

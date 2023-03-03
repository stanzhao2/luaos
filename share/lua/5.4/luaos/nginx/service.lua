

--[[
*********************************************************************************
** 
** Copyright 2021-2023 LuaOS
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
*********************************************************************************
]]--

local luaos = require("luaos");
local nginx = require("luaos.nginx");
local certificates = {};

--Create a TLS context for the specified hostname
local function tls_context(hostname)
    local hostcert = certificates[hostname];
    if not hostcert then
        return nil;
    end
    local context = tls.context();
    assert(context:load(hostcert.cert, hostcert.key, hostcert.passwd));
    return context;
end

function main(host, port, root, certs)
    if not certs then
        tls_context = nil;
    else
        certificates = certs;
    end
    
	local nginx, result = luaos.nginx.start(host, port, root, tls_context);
    assert(nginx, result);    
    nginx:upgrade(); --websocket enabled
    
	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end	
    
	nginx:stop();
end

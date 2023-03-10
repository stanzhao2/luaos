

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

local handler = nil;
local luaos   = require("luaos");
local pack    = luaos.conv.pack;
local hash    = luaos.conv.hash;
local format  = string.format;

----------------------------------------------------------------------------

local function on_receive_from(ec, data, from)
    if ec > 0 then
        error(format("log receive error: %d", ec));
        return;
    end
    local packet = pack.decode(data);
    if handler and packet then
        local s = pack.encode(from);
        luaos.publish(handler, hash.crc32(s), 0, from, packet);
    end
end

----------------------------------------------------------------------------

function main(host, port, topic)
    local socket = luaos.socket("udp");
    assert(type(topic) == "number")
    assert(socket and host and port);
    handler = topic;
    
    local ok, err = socket:bind(host, port, on_receive_from);
    if not ok then
        error(err);
        return;
    end
    
	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end    
    socket:close();
end

----------------------------------------------------------------------------

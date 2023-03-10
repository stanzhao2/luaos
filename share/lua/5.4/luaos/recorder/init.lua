

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

local luaos  = require("luaos");
local pack   = luaos.conv.pack;
local hash   = luaos.conv.hash;
local topic  = 0x1122334455667788;
local format = string.format;

----------------------------------------------------------------------------

local function on_receive_from(ec, data, from)
    if ec > 0 then
        error(format("log receive error: %d", ec));
        return;
    end
    local ok, packet = pcall(pack.decode, data);
    if ok and type(packet) == "table" then
        local mask = hash.crc32(from.ip);
        luaos.publish(topic, mask, 0, from, packet);
    end
end

----------------------------------------------------------------------------

--The module can be started independently
function main(host, port, count)
    local socket = luaos.socket("udp");
    assert(socket and host and port);
    
    --Prevent starting multiple
    if luaos.global.get(tostring(topic)) then
        return;
    end
    
    local ok, err = socket:bind(host, port, on_receive_from);
    if not ok then
        error(err);
        return;
    end
    
    local workers = {};
    for i = 1, math.min(count or 1, 32) do
        local worker = luaos.start("luaos.recorder.worker", topic);
        if worker then
            table.insert(workers, worker);
        end
    end
    
    luaos.global.set(tostring(topic), true);
	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end
    
    socket:close();
    for i = 1, #workers do
        workers[i]:stop();
    end
end

----------------------------------------------------------------------------

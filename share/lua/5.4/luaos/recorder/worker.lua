

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
local format = string.format;

----------------------------------------------------------------------------

local function check_data(data)
    if type(data.module) ~= "string" then
        return false;
    end
    if type(data.type) ~= "string" then
        return false;
    end
    if type(data.message) ~= "string" then
        return false;
    end
    return true;
end

local function on_publish(publisher, mask, from, data)
    if not check_data(data) then
        return;
    end
    local date = os.date("*t");
    local path = format("log/%d%02d%02d", date.year, date.month, date.day);
    os.mkdir(path);
    
    local filename = format("%s/%s.log", path, from.ip);
    local log = io.open(filename, "a");
    if not log then
        return;
    end
    
    local message = format("<%s:%s> %s", data.module, data.type, data.message);
    log:write(message);
    log:close();
end

----------------------------------------------------------------------------

function main(topic)
    assert(topic);
    if not luaos.subscribe(topic, on_publish) then
        return;
    end
	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end
    luaos.cancel(topic);
end

----------------------------------------------------------------------------

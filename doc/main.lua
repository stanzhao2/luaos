

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
local nginx  = require("luaos.nginx");
local event  = nginx.event;
local bind   = luaos.bind;
local format = string.format;

----------------------------------------------------------------------------
--基础功能测试

print("OS name: "  .. luaos.typename());
print("ID: "       .. luaos.id());
trace("PID: "      .. luaos.pid());
error("UNIQUEID: " .. luaos.uniqueid());

local function on_files_callback(filename, extname)
    print(filename, extname);
end
luaos.files(on_files_callback, "./");

----------------------------------------------------------------------------
--定时器及时钟测试

local timer;
local function on_timer_callback(interval, ...)
    local system_time = luaos.system_clock();
    local steady_time = luaos.steady_clock();
    print(system_time, steady_time);
    timer = luaos.scheme(interval, bind(on_timer_callback, interval, ...));
end
timer = luaos.scheme(1000, bind(on_timer_callback, 1000, "abc"));

----------------------------------------------------------------------------
--全局存储测试

local function on_global_set(new_value, original)
    if original ~= new_value then
        return new_value;
    end
    return original;
end

local global = luaos.global;
global.set("abc", 1);
global.set("abc", bind(on_global_set, 2));
print(global.get("abc"));

----------------------------------------------------------------------------

--[[
--If you need https/wss communication, you need to cancel the comment
local certificates = {
    ["hostname"] = {
        cert   = "",  --Certificate file name
        key    = "",  --Certificate key file name
        passwd = nil, --Password of key file
    },
};
]]--

----------------------------------------------------------------------------
--网络/订阅发布机制测试

local function on_receive(fd, data, opcode)
    nginx.send(fd, data, opcode);
end

----------------------------------------------------------------------------

local function on_error(fd, ec)
    trace(format("Websocket connection error, id=%d code=%d", fd, ec));
end

----------------------------------------------------------------------------

local function on_accept(fd, headers, from, port)
    trace(format("Websocket accepted, id=%d from %s:%d", fd, from, port));
    for k, v in pairs(headers) do
        trace(k, v);
    end
end

----------------------------------------------------------------------------

function main(...)
    nginx.on(event.accept,  on_accept)
         .on(event.error,   on_error)
         .on(event.receive, on_receive)
         .listen("0.0.0.0", 8899, certificates);

	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end	
    
	nginx.close();
end

----------------------------------------------------------------------------

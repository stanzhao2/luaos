#!/usr/local/bin/luaos

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
local format = string.format;

----------------------------------------------------------------------------

--[=[
--If you need https/wss communication, you need to cancel the comment
local certificates = {
    ["hostname"] = {
        cert   = "",  --Certificate file name
        key    = "",  --Certificate key file name
        passwd = nil, --Password of key file
    },
};
--]=]

----------------------------------------------------------------------------

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

function main(host, port, wwwroot)    
    nginx.on(nginx.event.accept,  on_accept)
         .on(nginx.event.error,   on_error)
         .on(nginx.event.receive, on_receive)
         .listen(host or "0.0.0.0", port or 8899, wwwroot or "wwwroot", certificates);

	while not luaos.stopped() do
		local success, err = pcall(luaos.wait);
        if not success then
            error(err);
        end
	end
    
    nginx.close();
end

----------------------------------------------------------------------------

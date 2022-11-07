

--[[
*********************************************************************************
** 
** Copyright 2021-2022 LuaOS
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

----------------------------------------------------------------------------

local class = require("luaos.classy")
local pump_message = class("pump_message");
local skiplist = require("skiplist")

----------------------------------------------------------------------------


local function on_error(err)
    error(debug.traceback(err));
end

----------------------------------------------------------------------------

function pump_message:__init(...)
    self.handlers = {};
end

---注册一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
---@return function
function pump_message:register(name, handler, priority)
    assert(type(handler) == "function");
    local event = self.handlers[name];
    if not priority then
        priority = 0;
    end
    
    if not event then
        event = skiplist.new();
        self.handlers[name] = event;
    end
    
    event:insert(handler, priority);
    return handler;
end

---派发一个回调消息(仅当前模块)
---@param name integer|string
---@return integer
function pump_message:dispatch(name, ...)
    local event = self.handlers[name];
    if not event then
        return 0;
    end
    
    for i, handler in pairs(event:rank_range()) do
        xpcall(handler, on_error, ...);
    end
    
    return event:size();
end

---取消一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
function pump_message:unregister(name, handler)
    assert(type(handler) == "function");
    local event = self.handlers[name];
    if not event then
        return;
    end
    
    if event:delete(handler) and event:size() == 0 then
        self.handlers[name] = nil;
    end
end

----------------------------------------------------------------------------

return pump_message;

----------------------------------------------------------------------------

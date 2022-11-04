

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
local heap  = require("luaos.heap")

----------------------------------------------------------------------------

local pump_message = class("pump_message");

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
    
    if event == nil then
        event = heap.maxUnique();
        event:insert(handler, priority);
        self.handlers[name] = event;
        return handler;
    end
    
    event:remove(handler);
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
    
    local next = heap.maxUnique();
    while event:size() > 0 do
        local handler, priority = event:pop();
        self.callback = handler;
        
        xpcall(handler, on_error, ...);
        
        self.callback = nil;
        if self.removed == handler then
            self.removed = nil;
        else
            next:insert(handler, priority);
        end
    end
    
    self.handlers[name] = next;
    return next:size();
end

---取消一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
function pump_message:unregister(name, handler)
    assert(type(handler) == "function");
    local event = self.handlers[name];
    if event then
        if self.callback == handler then
            self.removed = handler;
        end
        
        local priority = event:remove(handler);
        if priority ~= nil and event:size() == 0 then
            self.handlers[name] = nil;
        end
    end
end

----------------------------------------------------------------------------

return pump_message;

----------------------------------------------------------------------------



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

----------------------------------------------------------------------------

local pump_message = class("pump_message");

----------------------------------------------------------------------------

function pump_message:__init(...)
    self.handlers = {};
end

---注册一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
---@return function
function pump_message:register(name, handler)
    assert(type(handler) == "function");
    local event = self.handlers[name];
    
    if event == nil then
        event = {};
        table.insert(event, handler);
        self.handlers[name] = event;
        return handler;
    end
    
    for _, v in ipairs(event) do
        if (v == handler) then
            return nil;
        end
    end
    
    table.insert(event, handler);
    return handler;
end

---取消一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
function pump_message:unregister(name, handler)
    local event = self.handlers[name];
    if not event then
        return;
    end
    
    local count = #event;
    for i = 1, count do
        if event[i] == handler then
            table.remove(event, i);
            if count == 1 then
                self.handlers[name] = nil;
            end
            break;
        end
    end
end

---派发一个回调消息(仅当前模块)
---@param name integer|string
---@return integer
function pump_message:dispatch(name, ...)
    local event = self.handlers[name];
    if not event then
        return 0;
    end
    
    local handlers = {};
    for _, handler in ipairs(event) do
        table.insert(handlers, handler);
    end
    
    local count = 0;
    for _, handler in ipairs(handlers) do
        handler(...);
        count = count + 1;
    end
    return count;
end

----------------------------------------------------------------------------

return pump_message;

----------------------------------------------------------------------------

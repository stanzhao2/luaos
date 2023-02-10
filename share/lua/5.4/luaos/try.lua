

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

local pcall = require("luaos.pcall");

local function is_function(v)
    return type(v) == "function"
end

local function finally(self, block, declared)
    local handler = block[1]
    if is_function(handler) then
        handler()
    end
    if not declared and not self.ok then
        error(self.err)
    end
end

local function catch(self, block)
    local handler  = block[1]
    local declared = is_function(handler)
    if not self.ok and declared then
        handler(self.err or "unknown error occurred")
    end
    return {
        finally = function(block)
            finally(self, block, declared)
        end
    }
end

local function begin()
    local t = {__commit = false, v = {}}
    function t:commit()
        self.__commit = true
    end
    function t:push(func)
        table.insert(self.v, func)
    end
    setmetatable(t, {__close = function(self) 
        if self.__commit == false then
            for i = #self.v, 1, -1 do
                self.v[i]()
            end
        end
    end})
    return t
end

local function try(self, block)
    if not block then
        return begin()
    end
    local t = {
        ok = true,
        err,
        catch,
        finally,
    }

    local handler = block[1]
    if is_function(handler) then
        t.ok, t.err = pcall(handler)
    end

    t.catch = function(block)
        return catch(t, block);
    end;

    t.finally = function(block)
        finally(t, block, false)
    end
    return t;
end

----------------------------------------------------------------------------

local M = {}

----------------------------------------------------------------------------

return setmetatable(M, {__call = try})

----------------------------------------------------------------------------

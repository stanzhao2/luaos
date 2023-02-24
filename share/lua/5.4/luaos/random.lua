

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

local genrand = require("random")
local class = require("luaos.classy")

---@class random
---@field private __init
---@field private __gc
local random = class("random")

function random:__init(gen)
    self.gen = gen
end

function random:__gc(gen)
    if self.gen then
        self.gen:close()
        self.gen = nil
    end
end

function random:close()
    if self.gen then
        self.gen:close();
    end
end

function random:srand(seed)
    if self.gen then
        self.gen:srand(seed)
    end
end

function random:random(...)
    if not self.gen then
        return nil
    end
    return self.gen:generate(...)
end

----------------------------------------------------------------------------
local x = {};

---@return random 返回一个32随机数发生器
function x.random32_create()
    return random(genrand.random32());
end

---@return random 返回一个64随机数发生器
function x.random64_create()
    return random(genrand.random64());
end

----------------------------------------------------------------------------
return x

----------------------------------------------------------------------------

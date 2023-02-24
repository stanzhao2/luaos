

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

----------------------------------------------------------------------------

local function bind(callback, ...)
    assert(type(callback) == "function")
    local params = {...}
    local n = #params
    if n == 0 then
        return function(...)
            return callback(...)
        end
    elseif n == 1 then
        return function(...)
            return callback(params[1], ...)
        end
    elseif n == 2 then
        return function(...)
            return callback(params[1], params[2], ...)
        end
    elseif n == 3 then
        return function(...)
            return callback(params[1], params[2], params[3], ...)
        end
    end
    return function(...)
        table.insert(params, ... or nil)
        return callback(table.unpack(params))
    end
end

return bind

----------------------------------------------------------------------------

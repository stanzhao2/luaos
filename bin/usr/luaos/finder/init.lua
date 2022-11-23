

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

local astar = {}

local jumper = "luaos.finder.jumper."

local switch = {
	["grid"] = function(...)
		local grid   = require(jumper .. "grid")
		return grid(...)
	end,
	["finder"] = function(...)
		local finder = require(jumper .. "pathfinder")
		return finder(...)
	end
}

--[[
-- Value for walkable tiles
local walkable = 1
-- Creates a grid object
local grid = astar.new("grid", map)
-- Creates a pathfinder object using Jump Point Search
local finder = astar.new("finder", grid, 'ASTAR', walkable) 
--finder:setHeuristic('MANHATTAN')
local path = finder:getPath(startx, starty, endx, endy)
]]--

----------------------------------------------------------------------------

function astar.new(name, ...)
	local fn = switch[name]
	if fn == nil then return nil end
	return fn(...)
end

----------------------------------------------------------------------------

return astar

----------------------------------------------------------------------------

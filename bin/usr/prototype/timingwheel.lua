

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

---@class luaos_timingwheel
local i_timingwheel = {};

---添加一个计划,成功返回计划 id，否则返回 nil
---@param millisecond integer
---@handler fun():void
---@return integer:nil
function i_timingwheel:scheme(millisecond, handler) end;

---关闭时间轮
function i_timingwheel:close() end;

---取消一个设定的计划
---@param id integer
function i_timingwheel:cancel(id) end;

---获取时间轮允许的最大时间值(毫秒),成功返回允许的最大时间值，否则返回 nil
---@return integer|nil
function i_timingwheel:max_delay() end;

----------------------------------------------------------------------------

return i_timingwheel;

----------------------------------------------------------------------------

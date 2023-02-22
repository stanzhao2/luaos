

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

---@class tls_context
local context = {};

---设置ca验证证书
---@param cafile string
---@return boolean|nil
function context:ca(cafile) end

---设置本地 TLS 证书
---@param certdata string
---@param keydata string
---@param passwd string
---@return boolean|nil
function context:assign(certdata, keydata, passwd) end

---设置本地 TLS 证书
---@param certfile string
---@param keyfile string
---@param passwd string
---@return boolean|nil
function context:load(certfile, keyfile, passwd) end
 
---设置 TLS sni 回调函数
---@param callback fun(hostname:string)
---@return boolean|nil
function context:sni_callback(callback) end

---关闭 TLS 上下文
function context:close() end

----------------------------------------------------------------------------

return context;

----------------------------------------------------------------------------

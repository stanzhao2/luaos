

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

local curl = require("curl")

----------------------------------------------------------------------------

---获取 url 页面内容
function curl.wget(url, ...)
    local result = {}
    local c = curl.new()
        c:setopt(curl.OPT_URL, url)
		c:setopt(curl.OPT_FOLLOWLOCATION, true)
        c:setopt(curl.OPT_SSL_VERIFYPEER, false) --不验证对端证书
        c:setopt(curl.OPT_WRITEDATA, result)
        c:setopt(curl.OPT_WRITEFUNCTION,
			function(tab, buffer)
				table.insert(tab, buffer)
				return #buffer
			end
		)
		if ... and #... > 0 then
			c:setopt(curl.OPT_HTTPHEADER, ...)
		end
		--c:setopt(curl.OPT_VERBOSE, true) --设置为true在执行时打印请求信息
		--c:setopt(curl.OPT_HEADER,  true) --设置为true将响应头信息同响应体一起传给WRITEFUNCTION
    local ok, err = c:perform()
    c:close()
	c = nil
	if (ok == false) then
		trace("curl perform failed: " .. err)
	end
    return ok, table.concat(result)
end

----------------------------------------------------------------------------

---获取 url 页面内容
function curl.wpost(url, data, ...)
    local result = {}
    local c = curl.new()
        c:setopt(curl.OPT_URL, url)
		c:setopt(curl.OPT_POST, true)
		c:setopt(curl.OPT_FOLLOWLOCATION, true)
        c:setopt(curl.OPT_SSL_VERIFYPEER, false) --不验证对端证书
		c:setopt(curl.OPT_POSTFIELDS, data)
        c:setopt(curl.OPT_WRITEDATA, result)
        c:setopt(curl.OPT_WRITEFUNCTION,
			function(tab, buffer)
				table.insert(tab, buffer)
				return #buffer
			end
		)
		if ... and #... > 0 then
			c:setopt(curl.OPT_HTTPHEADER, ...)
		end
		--c:setopt(curl.OPT_VERBOSE, true) --设置为true在执行时打印请求信息
		--c:setopt(curl.OPT_HEADER,  true) --设置为true将响应头信息同响应体一起传给WRITEFUNCTION
    local ok, err = c:perform()
    c:close()
	c = nil
	if (ok == false) then
		trace("curl perform failed: " .. err)
	end
    return ok, table.concat(result)
end

----------------------------------------------------------------------------

return curl

----------------------------------------------------------------------------


local luaos = require("luaos")

local format = string.format

local html = [[
<html>
	<head>
		<title>LuaOS v5.4.4</title>
	</head>
	<body>
		<table width="600">
		<tr>
			<td border=0 width="80">
			<img src="images/luaos.gif" width="64", height="64" border="0" />
			</td>
			<td>
			<b><i>LuaOS for %s is working</i></b>
			</td>
		</tr>
		<tr>
			<td colspan="2"><hr></td>
		</tr>
		<tr>
			<td colspan="2">
			<font size="1">
				THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
				IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
				FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
				AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
				LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
				OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
				SOFTWARE.
			</font>
			</td>
		</tr>
		</table>
	</body>
</html>
]]

----------------------------------------------------------------------------

local function on_http_request(request, response, params)
	local data = format(html, luaos.typename());
	response:write(data);
end

----------------------------------------------------------------------------

local function on_ws_accept(peer)
	
end

----------------------------------------------------------------------------

local function on_ws_request(peer, ec, data, opcode, deflate)
	if ec > 0 then
		trace("websocket error, errno: ", ec);
		return;
	end
	peer:send(data, opcode, deflate);
end

----------------------------------------------------------------------------
--以下代码为标准接口, 不要修改

local nginx_http = {};

---HTTP 请求
function nginx_http.on_request(request, response, params)
	on_http_request(request, response, params);
end

---Websocket 连接
function nginx_http.on_accept(peer)
	on_ws_accept(peer);
end

---Websocket 请求
function nginx_http.on_receive(peer, ec, data, opcode, deflate)
	on_ws_request(peer, ec, data, opcode, deflate);
end

return nginx_http;

----------------------------------------------------------------------------

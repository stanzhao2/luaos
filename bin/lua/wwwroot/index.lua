
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

local format = string.format;

----------------------------------------------------------------------------

local function on_http_request(request, response, params)
	local data = format(html, luaos.typename());
	response:write(data);
end

----------------------------------------------------------------------------

local function on_ws_accept(ws_peer, request, params)
    local ip, port = ws_peer:endpoint();
    trace(format("websocket accept from %s", ip));
end

----------------------------------------------------------------------------

local function on_ws_request(ws_peer, ec, data, opcode)
	if ec > 0 then
		trace(format("websocket error, errno: %d", ec));
		return;
	end
	ws_peer:send(data, opcode);
end

----------------------------------------------------------------------------
--以下代码为标准接口, 不要修改

local nginx_http = {};

---HTTP GET/POST 请求
function nginx_http.on_request(request, response, params)
	on_http_request(request, response, params);
end

---Websocket 连接
function nginx_http.on_accept(ws_peer, request, params)
	on_ws_accept(ws_peer, request, params);
end

---Websocket 请求
function nginx_http.on_receive(ws_peer, ec, data, opcode)
	on_ws_request(ws_peer, ec, data, opcode);
end

return nginx_http;

----------------------------------------------------------------------------

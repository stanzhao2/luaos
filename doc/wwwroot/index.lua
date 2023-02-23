

local luaos     = require("luaos")
local nginx = require("luaos.nginx");
local format    = string.format;

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

---Internal function, do not call
function nginx.on_request(request, response, params)
	local data = format(html, luaos.typename());
	response:write(data);
	response:finish();
end

----------------------------------------------------------------------------

return nginx;

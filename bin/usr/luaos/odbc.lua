
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

local odbc = { mysql_env = nil };

----------------------------------------------------------------------------

function odbc.mysql(dbname, user, pwd, host, port)
    local ok, luasql = pcall(require, "luasql.mysql")
    if not ok then
        return nil
    end
    
    local conf = {
        dbname = dbname,
        user   = user,
        pwd    = pwd,
        host   = host,
        port   = port
    };
    
    if not odbc.mysql_env then
        odbc.mysql_env = luasql.mysql();
    end
    
    local mysql = require("luaos.odbc.mysql")
    return mysql(odbc.mysql_env, conf);    
end

----------------------------------------------------------------------------

return odbc

----------------------------------------------------------------------------

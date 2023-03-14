
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

local class  = require("luaos.classy")
local format = string.format;
local mysql  = class("mysql");

----------------------------------------------------------------------------

local function connect(sqlenv, conf)
    local conn, errmsg = sqlenv:connect(
        conf.dbname, conf.user, conf.pwd, conf.host, conf.port
    );
    if not conf.port then
        conf.port = 3306;
    end
    if not conf.charset then
        conf.charset = "utf8mb4";
    end
    if conn then
        conn:execute(format("set names %s", conf.charset));
    end
    return conn, errmsg;
end

function mysql:__init(sqlenv, conf)
    self.conf   = conf;
    self.sqlenv = sqlenv;
    self.closed = false;
    assert(self:keepalive());
end

function mysql:__gc()
    self:close();
end

function mysql:__close()
    self:close();
end

function mysql:close()
    if self.conn then
        self.conn:close();
        self.conn = nil;
    end
    self.closed = true;
end

function mysql:is_open()
    return self.conn ~= nil and not self.closed;
end

function mysql:begin()
    if not self.conn then
        return nil;
    end    
    local pending = { conn = self.conn };    
    function pending.commit(self)
        assert(self);
        self.conn:commit();
        self.conn = nil;
    end    
    self.conn:execute("start transaction");
    return setmetatable(pending, {
        __close = function(holder)
            if holder.conn then
                holder.conn:rollback();
            end
        end
        }
    );
end

function mysql:lastid()
    if not self.conn then
        return nil;
    end
    return self.conn:getlastautoid();
end

function mysql:execute(sql, ...)    
    if not self:keepalive() then
        return nil;
    end
    local stmt = nil;
    local result = nil;
    try {
        bind(function(...)
            stmt = self.conn:prepare(sql);
            assert(stmt and stmt:bind(...));
            result = assert(stmt:execute({}));
        end, ...)
    }
    .catch {
        bind(function(...)
            error(sql, ...);
        end, ...)
    }
    .finally {
        function()
            if stmt then stmt:close() end
        end
    }
    return result;
end

function mysql:keepalive()
    if self.closed then
        return false;
    end    
    if self.conn then
        if not self.conn:ping() then
            self.conn:close();
            self.conn = nil;
        end
    end    
    if not self.conn then
        local ok, conn, errmsg = pcall(connect, self.sqlenv, self.conf);
        if not ok or not conn then
            error(conn or errmsg);
            return false;
        end
        self.conn = conn;
    end    
    return self.conn ~= nil;
end

----------------------------------------------------------------------------

return mysql

----------------------------------------------------------------------------

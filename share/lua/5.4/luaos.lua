

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
---封装 luaos 主模块
----------------------------------------------------------------------------

dofile("luaos.global");

---Save to local variables for efficiency
local ok;
local io    = io;
local os    = os;
local tls   = tls;

---@class luaos
local luaos = {
    read = 1, write = 2,
    
    ---创建一个 socket
    ---@param family string
    ---@return luaos_socket
    socket = function(family)
        return io.socket(family);
    end,
    
    ---获取当前 UTC 时间(精确到毫秒)
    system_clock = function()
        return os.system_clock();
    end,
    
    ---获取单调时钟(精确到毫秒)
    steady_clock = function()
        return os.steady_clock();
    end,
    
    ---获取当前操作系统类型名
    ---@return string
    typename = function()
        return os.typename();
    end,
    
    ---获取当前系统硬件唯一码
    ---@return string
    uniqueid = function()
        return os.uniqueid();
    end,
    
    ---遍历目录下所有文件
    ---@param handler fun(filename:string, ext:string):void
    ---@param path string
    ---@param ext string
    files = function(handler, path, ext)
        return os.files(handler, path, ext);
    end,
    
    ---设置一个 Timer
    ---@param expires integer
    ---@callback function
    ---@return userdata
    scheme = function(expires, callback)
        return os.scheme(expires, callback);
    end,
    
    ---执行一个 lua 模块
    ---@param name string
    ---@return luaos_job
    start = function(name, ...)
        return os.start(name, ...);
    end,
    
    ---优雅退出当前 lua 模块运行
    exit = function()
        return os.exit();
    end,
    
    ---等待系统消息,返回执行的消息数量
    ---@param timeout integer
    ---@return integer
    wait = function(timeout)
        return os.wait(timeout or -1);
    end,
    
    ---判断当前 lua 虚拟机是否还在运行
    ---@return boolean
    stopped = function()
        return os.stopped();
    end,
    
    ---发布一个系统消息(跨模块)
    ---@param topic integer
    ---@param mask integer
    ---@param receiver integer
    ---@return integer
    publish = function(topic, mask, receiver, ...)
        return os.publish(topic, mask, receiver, ...);
    end,
    
    ---取消一个消息订阅(跨模块)
    ---@param topic integer
    cancel = function(topic)
        return os.cancel(topic);
    end,
    
    ---监视一个消息订阅(跨模块)
    ---@param topic integer
    ---@param handler fun(subscriber:integer, type:string):void
    ---@return boolean
    watch = function(topic, handler)
        return os.watch(topic, handler);
    end,
    
    ---订阅一个系统消息(跨模块)
    ---@param topic integer
    ---@param handler fun(publisher:integer, mask:integer, ...):void
    ---@return boolean
    subscribe = function(topic, handler)
        return os.subscribe(topic, handler);
    end,
};

ok, luaos.bind  = pcall(require, "luaos.bind");
if not ok then
    throw(luaos.bind);
end

ok, luaos.try   = pcall(require, "luaos.try");
if not ok then
    throw(luaos.try);
end

ok, luaos.conv  = pcall(require, "luaos.conv");
if not ok then
    throw(luaos.conv);
end

ok, luaos.curl  = pcall(require, "luaos.curl");
if not ok then
    throw(luaos.curl);
end

ok, luaos.odbc  = pcall(require, "luaos.odbc");
if not ok then
    throw(luaos.odbc);
end

ok, luaos.class = pcall(require, "luaos.classy");
if not ok then
    throw(luaos.class);
end

----------------------------------------------------------------------------
---封装 TLS 子模块
----------------------------------------------------------------------------

luaos.tls = {
    ---创建一个 TLS context
    ---@param  certfile string
    ---@param  keyfile string
    ---@param  keypwd string
    ---@return tls_context
    context = function(certfile, keyfile, keypwd)
        if not tls then
            return nil;
        end
        return tls.context(certfile, keyfile, keypwd);
    end
};

----------------------------------------------------------------------------
---封装 nginx/http 子模块
----------------------------------------------------------------------------

luaos.nginx = {
    ---启动一个 http 服务模块
    ---@param host string
    ---@param port integer
    ---@param wwwroot string
    ---@param sslctx fun(hostname:string)
    ---@return table
    start = function(host, port, wwwroot, sslctx)
        local ok, server = pcall(
            require, "luaos.nginx"
        );
        
        if not ok then
            throw(server);
        end
        
        local ok, reason = server.start(host, port, wwwroot, sslctx);
        if not ok then
            return nil, reason;
        end
        
        local result = {};
        
        function result:stop()
            server.stop();
        end
        
        function result:upgrade()
            server.upgrade();
        end
        
        return result;
    end
};

----------------------------------------------------------------------------
---封装 cluster 子模块
----------------------------------------------------------------------------

luaos.cluster = {
    ---创建一个集群监听
    ---@param host string
    ---@param port integer
    ---@return table
    listen = function(host, port)
        local ok, master = pcall(
            require, "luaos.cluster.master"
        );
        
        if not ok then
            throw(master);
        end
        
        local ok, reason = master.start(host, port);
        if not ok then
            return nil, reason;
        end
        
        local result = {};
        
        function result:close()
            master.stop();
        end
        
        return result;
    end,
    
    ---建立一个集群连接
    ---@param host string
    ---@param port integer
    ---@param timeout integer
    ---@return table
    connect = function(host, port, timeout)
        local ok, proxy = pcall(
            require, "luaos.cluster.proxy"
        );
        
        if not ok then
            throw(proxy);
        end
        
        local ok, reason = proxy.start(host, port, timeout);
        if not ok then
            return nil, reason;
        end
        
        local result = {};
        
        function result:close()
            proxy.stop();
        end
        
        function result:watch(topic)
            proxy.watch(topic);
        end
        
        return result;
    end
};

----------------------------------------------------------------------------
---封装 bigint 子模块
----------------------------------------------------------------------------

local bigint;
ok, bigint = pcall(require, "bigint");
if not ok then
    throw(bigint);
end

---创建一个大整数
---@param v integer
math.bigint = function(v)
    return bigint.new(v);
end

----------------------------------------------------------------------------
---封装 random 子模块
----------------------------------------------------------------------------

local random;
ok, random = pcall(require, "luaos.random");
if not ok then
    throw(random);
end

local random32 = random.random32_create();
local random64 = random.random64_create();

---创建一个 32 位随机数
---@retrun integer
math.random32 = function(...)
    return random32:random(...);
end

---创建一个 64 位随机数
---@retrun integer
math.random64 = function(...)
    return random64:random(...);
end

----------------------------------------------------------------------------
---封装 pump_message 子模块
----------------------------------------------------------------------------

local class_pump, private_pump;
ok, class_pump = pcall(require, "luaos.pump");
if not ok then
    throw(class_pump);
else
    private_pump = class_pump();
end

---创建一个消息反应堆
luaos.pump_message = function()
    return class_pump();
end

---注册一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
---@return function
luaos.register = function(name, handler, priority)
    return private_pump:register(name, handler, priority);
end

---取消一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
luaos.unregister = function(name, handler)
    return private_pump:unregister(name, handler);
end

---派发一个回调消息(仅当前模块)
---@param name integer|string
---@return integer
luaos.dispatch = function(name, ...)
    return private_pump:dispatch(name, ...);
end

----------------------------------------------------------------------------
---设置元表，禁止添加新元素
----------------------------------------------------------------------------

if not _DEBUG then
    return luaos;
end

return setmetatable({name = "LuaOS"}, {
    __index    = luaos,
    __newindex = function (t, k, v) throw("error write to a read-only table with key = " .. tostring(k)) end, 
    __pairs    = function (t) return pairs(luaos) end, 
    __len      = function (t) return #luaos end, 
});

----------------------------------------------------------------------------



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

pcall(dofile, "luaos.global");

---Save to local variables for efficiency
local ok;
local storage   = storage;
local ssl       = ssl;
local rpc       = rpc;
local io        = io;
local os        = os;

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
    
    ---遍历目录下所有文件
    ---@param handler fun(filename:string, ext:string):void
    ---@param path string
    ---@param ext string
    files = function(handler, path, ext)
        return os.files(handler, path, ext);
    end,
    
    ---创建一个时间轮
    ---@return luaos_timingwheel
    timingwheel = function()
        return os.timingwheel();
    end,
    
    ---获取当前操作系统类型名
    ---@return string
    typename = function()
        return os.typename();
    end,
    
    ---获取当前 lua 虚拟机编号
    ---@return integer
    id = function()
        return os.id();
    end,
    
    ---执行一个 lua 模块
    ---@param name string
    ---@return luaos_job
    execute = function(name, ...)
        return os.execute(name, ...);
    end,
    
    ---优雅退出当前 lua 模块运行
    exit = function()
        return os.exit();
    end,
    
    ---设置全局脚本最大阻塞时间(防止出现死循环)
    ---@param seconds integer
    deadline = function(seconds)
        return os.deadline(seconds);
    end,
    
    ---等待系统消息,返回执行的消息数量
    ---@param timeout integer
    ---@return integer
    wait = function(timeout)
        return os.wait(timeout);
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

ok, luaos.pcall = pcall(require, "luaos.pcall");
ok, luaos.bind  = pcall(require, "luaos.bind");
ok, luaos.try   = pcall(require, "luaos.try");
ok, luaos.conv  = pcall(require, "luaos.conv");
ok, luaos.curl  = pcall(require, "luaos.curl");
ok, luaos.odbc  = pcall(require, "luaos.odbc");
ok, luaos.class = pcall(require, "luaos.classy");

----------------------------------------------------------------------------
---封装 storage 子模块
----------------------------------------------------------------------------

local pack = luaos.conv
if pack then
    pack = luaos.conv.pack;
end

luaos.storage = {
    ---设置一个 key-value 对
    ---@overload fun(key:string, handler:fun(old_value:any):any):any
    ---@param key string
    ---@param value any
    ---@retrun any
    set = function(key, value)
        key = tostring(key);
        if not pack then
            return storage.set(key, value);
        end
        
        local ty = type(value)
        if ty ~= "function" then
            value = pack.encode(value);
            value = storage.set(key, value);
            if value then
                value = pack.decode(value);
            end
            return value;
        end
        
        local luaold;
        storage.set(key, function(old)
            if pack and old then
                old = pack.decode(old);
            end
            luaold = old;
            return pack.encode(value(old))
        end);
        
        return luaold;
    end,
    
    ---获取一个 key-value 值
    ---@param key string
    ---@return any
    get = function(key)
        key = tostring(key);        
        local value = storage.get(key);
        
        if pack and value then
            value = pack.decode(value)
        end
        
        return value;
    end,
    
    ---清除所有 key-value 值
    clear = function()
        return storage.clear();
    end,
    
    ---擦除指定的 key-value 值
    ---@param key string
    ---@return any
    erase = function(key)
        key = tostring(key);        
        local value = storage.erase();
        
        if pack and value then
            value = pack.decode(value)
        end
        
        return value;
    end,
};

----------------------------------------------------------------------------
---封装 SSL/TLS 子模块
----------------------------------------------------------------------------

luaos.ssl = {
    ---创建一个 SSL context
    ---@param cert string
    ---@param key string
    ---@param pwd string
    ---@return userdata
    context = function(cert, key, pwd)
        if not ssl then
            return nil;
        end
        return ssl.context(cert, key, pwd);
    end
};

----------------------------------------------------------------------------
---封装 rpcall 子模块
----------------------------------------------------------------------------

luaos.rpcall = setmetatable({
    ---注册一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@param func function
    ---@return boolean
    register = function(name, func)
        return rpc.register(name, func);
    end,
    
    ---取消一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@return boolean
    unregister = function(name)
        return rpc.cancel(name);
    end,
    }, {
    ---同步执行一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@param callback [function]
    ---@return boolean,...
    __call = function(_, name, callback, ...)
        return rpc.call(name, callback, ...);
    end
});

----------------------------------------------------------------------------
---封装 nginx/http 子模块
----------------------------------------------------------------------------

luaos.nginx = {
    ---启动一个 http 服务模块
    ---@param host string
    ---@param port integer
    ---@param wwwroot string
    ---@param ctx userdata
    ---@return table
    start = function(host, port, wwwroot, ctx)
        local ok, server = pcall(
            require, "luaos.nginx"
        );
        
        if not ok then
            throw("module nginx is not installed");
        end
        
        local ok, reason = server.start(host, port, wwwroot, ctx);
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
            throw("module master is not installed");
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
            throw("module proxy is not installed");
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

local function bigint_check()
    if not bigint then
        throw("module bigint is not installed");
    end
end

---创建一个大整数
---@param v integer
math.bigint = function(v)
    return bigint_check() or bigint.new(v);
end

----------------------------------------------------------------------------
---封装 random 子模块
----------------------------------------------------------------------------

local random;
ok, random = pcall(require, "luaos.random");

local random32, random64;
if random then
    random32 = random.random32_create();
    random64 = random.random64_create();
end

local function random_check()
    if not random then
        throw("module random is not installed");
    end
end

---创建一个 32 位随机数
---@retrun integer
math.random32 = function(...)
    return random_check() or random32:random(...);
end

---创建一个 64 位随机数
---@retrun integer
math.random64 = function(...)
    return random_check() or random64:random(...);
end

----------------------------------------------------------------------------
---封装 pump_message 子模块
----------------------------------------------------------------------------

local class_pump, private_pump;
ok, class_pump = pcall(require, "luaos.pump");

if class_pump then
    private_pump = class_pump();
end

local function pump_check()
    if not class_pump then
        throw("module pump is not installed");
    end
end

---创建一个消息反应堆
luaos.pump_message = function()
    return pump_check() or class_pump();
end

---注册一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
---@return function
luaos.register = function(name, handler, priority)
    return pump_check() or private_pump:register(name, handler, priority);
end

---取消一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
luaos.unregister = function(name, handler)
    return pump_check() or private_pump:unregister(name, handler);
end

---派发一个回调消息(仅当前模块)
---@param name integer|string
---@return integer
luaos.dispatch = function(name, ...)
    return pump_check() or private_pump:dispatch(name, ...);
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

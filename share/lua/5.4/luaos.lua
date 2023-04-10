﻿

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

dofile("luaos.misc");

----------------------------------------------------------------------------
---封装 luaos 主模块
----------------------------------------------------------------------------

---Save to local variables for efficiency
local io  = io;
local os  = os;
local tls = tls;

----------------------------------------------------------------------------
---替换系统 pairs 函数
----------------------------------------------------------------------------

local __pairs = pairs;

local function lt(a, b)
    if tostring(a) < tostring(b) then 
        return true
    end
end

local function gt(a, b)
    if tostring(a) > tostring(b) then 
        return true
    end
end

pairs = function(t, f)
    if f == nil then
        return __pairs(t);
    end
    local a = {}
    for k in __pairs(t) do table.insert(a, k) end
    if type(f) == "boolean" then
        if f then
            f = lt
        else
            f = gt
        end
    end
    table.sort(a, f)
    local i = 0                 -- iterator variable
    local iter = function ()    -- iterator function
       i = i + 1
       if a[i] == nil then return nil
       else return a[i], t[a[i]]
       end
    end
    return iter
end

----------------------------------------------------------------------------
---封装 luaos 模块
----------------------------------------------------------------------------

---@class luaos
local luaos = {
    read  = io.socket.read,

    write = io.socket.write,
    
    ---给函数绑定参数
    ---@param  fn function
    ---@return function
    bind = function(fn, ...)
        return bind(fn, ...);
    end,
    
    ---尝试执行指定的函数(函数必须放在table里)
    ---@param  fntry table
    ---@return table
    try = function(fntry)
        return try(fntry);
    end,
    
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
    
    ---获取当前模块 ID
    ---@return integer
    id = function()
        return os.id();
    end,
    
    ---获取当前模块父 ID
    ---@return integer
    pid = function()
        return os.pid();
    end,
    
    ---获取雪花码
    ---@param uid integer|nil
    ---@return integer
    snowid = function(uid)
        return os.snowid(uid);
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

----------------------------------------------------------------------------
---加载 general 模块
----------------------------------------------------------------------------

local modules = {
    conv    =   "conv",     --lua conv
    curl    =   "curl",     --lua curl
    odbc    =   "odbc",     --lua odbc
    class   =   "classy",   --lua classy
}

luaos.list = require("list");

for k, v in pairs(modules) do
    local ok, result = pcall(require, "luaos." .. v);
    if not ok then
        throw(result);
    end
    luaos[k] = result;
end

----------------------------------------------------------------------------
---封装 rpcall 子模块
----------------------------------------------------------------------------

local __rpcall = rpcall;
rpcall = nil;  --disable original rpcall

luaos.rpcall = setmetatable({
    ---注册一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@param func function
    ---@return boolean
    register = function(name, func)
        return __rpcall.register(name, func);
    end,
    
    ---取消一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@return boolean
    unregister = function(name)
        return __rpcall.cancel(name);
    end,
    }, {
    ---执行一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@param callback [function]
    ---@return boolean,...
    __call = function(_, name, callback, ...)
        return __rpcall.call(name, callback, ...);
    end
});

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
            require, "luaos.nginx.httpd"
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

local function cluster_watch(proxy, topics)
    local result = {
        proxy = proxy,
        close = proxy.stop,
        watch = proxy.watch,
    };
    if not topics then
        return result;
    end
    
    if type(topics) == "number" then
        proxy.watch(topics);
        return result;
    end
    
    if type(topics) == "table" then
        for _, v in pairs(topics) do
            proxy.watch(v);
        end
        return result;
    end
    
    error("invalid parameter");
    return result;
end

luaos.cluster = {
    ---建立一个集群连接
    ---@param host string
    ---@param port integer
    ---@param topics integer|table|nil
    ---@return table
    connect = function(host, port, topics)
        assert(host and port);
        local ok, proxy = pcall(
            require, "luaos.cluster.proxy"
        );
        if not ok then
            throw(proxy);
        end        
        local ok, reason = proxy.start(host, port, 2000);
        if not ok then
            return nil, reason;
        end        
        return true, cluster_watch(proxy, topics);
    end
};

----------------------------------------------------------------------------
---封装 pump_message 子模块
----------------------------------------------------------------------------

local global_pump;
local ok, pump = pcall(require, "luaos.pump");

if not ok then
    throw(pump);
else
    global_pump = pump();
end

---创建一个消息反应堆
luaos.pump_message = function()
    return pump();
end

---注册一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
---@return function
luaos.register = function(name, handler, priority)
    return global_pump:register(name, handler, priority);
end

---取消一个消息回调(仅当前模块)
---@param name integer|string
---@param handler fun(...):void
luaos.unregister = function(name, handler)
    return global_pump:unregister(name, handler);
end

---派发一个回调消息(仅当前模块)
---@param name integer|string
---@return integer
luaos.dispatch = function(name, ...)
    return global_pump:dispatch(name, ...);
end

----------------------------------------------------------------------------
---封装 global 子模块
----------------------------------------------------------------------------

local storage = global;
local pack = luaos.conv.pack;
global = nil;  --disable original global

luaos.global = {
    ---设置一个 key-value 对
    ---@param key string
    ---@param value any
    ---@param handler nil|fun(new:any, old:any):any
    ---@retrun any
    set = function(key, value, handler)
        return storage.set(key, value, handler);
    end,
    
    ---获取一个 key-value 值
    ---@param key string
    ---@return any
    get = function(key)
        return storage.get(key);
    end,
    
    ---清除所有 key-value 值
    clear = function()
        return storage.clear();
    end,
    
    ---擦除指定的 key-value 值
    ---@param key string
    ---@return any
    erase = function(key)
        return storage.erase(key);
    end,
};

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

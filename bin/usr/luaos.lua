

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
---为 string 添加功能
----------------------------------------------------------------------------

---去掉字符串前后空格
---@param str string
---@retrun string
string.trim = function(str)
    return (string.gsub(str, "^%s*(.-)%s*$", "%1")) ;
end

---将指定的字符串以指定的分隔符拆分
---@param str string
---@param seq string
---@retrun table
string.split = function(str, seq)
    if str == nil or type(str) ~= "string" then
        return nil;
    end
    
    local r, insert = {}, table.insert;
    
    for word in string.gmatch(str,"[^{"..seq.."}*]+") do
        insert(r, word);
    end
    
    return r
end

----------------------------------------------------------------------------
---为 table 添加功能
----------------------------------------------------------------------------

local insert   = table.insert;
local concat   = table.concat;
local format   = string.format;
local tostring = tostring;
local indents  = "    ";

local function format_table(data, level)
    local indent = "";
    for i = 1, level do
        indent = indent .. indents;
    end
    
    local result = {};
    if level == 0 then
        insert(result, "\n");
    end
    
    insert(result, "{\n");    
    for k, v in pairs(data) do
        insert(result, indent);
        
        local key = k;
        if type(k) == "number" then
            key = format("[%d]", k);
        end
        
        if type(v) == "table" then
            insert(result, format("%s%s = %s,\n", indents, key, format_table(v, level + 1)));
        else
            insert(result, format("%s%s = %s,\n", indents, key, tostring(v)));
        end
    end
    
    insert(result, format("%s}", indent));
    return concat(result);
end

---格式化 talbe
---@param data table
---@retrun string
table.format = function(data)
    assert(type(data) == "table");
    return format_table(data, 0);
end

----------------------------------------------------------------------------

local mt = {};

mt[mt] = setmetatable({}, {__mode = "k"});
mt.__mode = "kv";
mt.__index = function(t, k)
    local v = rawget(mt[mt][t], k);
    if type(v) == "table" then
        v = table.clone(v);
    end
    
    rawset(t, k, v);
    return v;
end

---克隆 table
---@param object table
---@param deep boolean
---@retrun table
table.clone = function(object, deep)
	if not deep then
		local ret = {};
		mt[mt][ret] = object;
		return setmetatable(ret, mt);
	end
    
	local lookup_table = {}
	local function _copy(object)
		if type(object) ~= "table" then
			return object;
		elseif lookup_table[object] then
			return lookup_table[object];
		end
        
		local new_table = {};
		lookup_table[object] = new_table;
        
		for key, value in pairs(object) do
			new_table[_copy(key)] = _copy(value);
		end
		return setmetatable(new_table, getmetatable(object));
	end
	return _copy(object);
end

----------------------------------------------------------------------------

local travelled = {};

---创建只读 table
---@param it table
---@retrun table
table.read_only = function(it)
    if not _DEBUG then
        return it;
    end
	local function __read_only(tbl) 
		if not travelled[tbl] then
			local tbl_mt = getmetatable(tbl) 
			if not tbl_mt then 
				tbl_mt = {} 
				setmetatable(tbl, tbl_mt) 
			end
			local proxy = tbl_mt.__read_only_proxy 
			if not proxy then 
				proxy = {} 
				tbl_mt.__read_only_proxy = proxy 
				local proxy_mt = {
					__index = tbl, 
					__newindex 	= function (t, k, v) throw("error write to a read-only table with key = " .. tostring(k)) end, 
					__pairs 	= function (t) return pairs(tbl) end, 
					__len 		= function (t) return #tbl end, 
					__read_only_proxy = proxy 
				} 
				setmetatable(proxy, proxy_mt) 
			end 
			travelled[tbl] = proxy 
			for k, v in pairs(tbl) do 
				if type(v) == "table" then 
					tbl[k] = __read_only(v) 
				end 
			end 
		end
		return travelled[tbl] 
	end
    return __read_only(it);
end

----------------------------------------------------------------------------
---封装 bigint 模块
----------------------------------------------------------------------------

local ok, bigint = pcall(require, "bigint");
if ok then
    ---创建一个大整数
    ---@param v integer
    math.bigint = function(v)
        return bigint.new(v);
    end
else
    trace("module bigint is not installed");
end

----------------------------------------------------------------------------
---封装系统内置模块
----------------------------------------------------------------------------

local private = {
    storage = {
        set      = storage.set,
        get      = storage.get,
        erase    = storage.erase,
        clear    = storage.clear,
    },
    
    ssl = {
        context = ssl.context,
    },
    
    rpc = {
        register = rpc.register,
        call     = rpc.call,
        cancel   = rpc.cancel,
    },
    
    socket       = io.socket,
    deadline     = os.deadline,
    files        = os.files,
    timingwheel  = os.timingwheel,
    typename     = os.typename,
    id           = os.id,
    execute      = os.execute,
    exit         = os.exit,
    wait         = os.wait,
    stopped      = os.stopped,
    publish      = os.publish,
    watch        = os.watch,
    cancel       = os.cancel,
    subscribe    = os.subscribe,
};

storage          = nil; --disused
ssl              = nil; --disused
rpc              = nil; --disused
io.socket        = nil; --disused
os.deadline      = nil; --disused
os.files         = nil; --disused
os.timingwheel   = nil; --disused
os.typename      = nil; --disused
os.id            = nil; --disused
os.execute       = nil; --disused
os.exit          = nil; --disused
os.wait          = nil; --disused
os.stopped       = nil; --disused
os.publish       = nil; --disused
os.cancel        = nil; --disused
os.watch         = nil; --disused
os.subscribe     = nil; --disused

----------------------------------------------------------------------------
---封装 luaos 主模块
----------------------------------------------------------------------------

local function on_error(err)
    error(debug.traceback(err));
end

---@class luaos
local luaos = {
    read = 1, write = 2,
    
    ---以保护模式运行函数
    ---@param func function
    ---@param ... any
    ---@return boolean,string
    pcall = function(func, ...)
        return xpcall(func, on_error, ...);
    end,
    
    ---创建一个 socket
    ---@param family string
    ---@return luaos_socket
    socket = function(family)
        return private.socket(family);
    end,
    
    ---遍历目录下所有文件
    ---@param handler fun(filename:string, ext:string):void
    ---@param path string
    ---@param ext string
    files = function(handler, path, ext)
        return private.files(handler, path, ext);
    end,
    
    ---创建一个时间轮
    ---@return luaos_timingwheel
    timingwheel = function()
        return private.timingwheel();
    end,
    
    ---获取当前操作系统类型名
    ---@return string
    typename = function()
        return private.typename();
    end,
    
    ---获取当前 lua 虚拟机编号
    ---@return integer
    id = function()
        return private.id();
    end,
    
    ---执行一个 lua 模块
    ---@param name string
    ---@return luaos_job
    execute = function(name, ...)
        return private.execute(name, ...);
    end,
    
    ---优雅退出当前 lua 模块运行
    exit = function()
        return private.exit();
    end,
    
    ---设置全局脚本最大阻塞时间(防止出现死循环)
    ---@param seconds integer
    deadline = function(seconds)
        return private.deadline(seconds);
    end,
    
    ---等待系统消息,返回执行的消息数量
    ---@param timeout integer
    ---@return integer
    wait = function(timeout)
        return private.wait(timeout);
    end,
    
    ---判断当前 lua 虚拟机是否还在运行
    ---@return boolean
    stopped = function()
        return private.stopped();
    end,
    
    ---发布一个系统消息(跨模块)
    ---@param topic integer
    ---@param mask integer
    ---@param receiver integer
    ---@return integer
    publish = function(topic, mask, receiver, ...)
        return private.publish(topic, mask, receiver, ...);
    end,
    
    ---取消一个消息订阅(跨模块)
    ---@param topic integer
    cancel = function(topic)
        return private.cancel(topic);
    end,
    
    ---监视一个消息订阅(跨模块)
    ---@param topic integer
    ---@param handler fun(subscriber:integer, type:string):void
    ---@return boolean
    watch = function(topic, handler)
        return private.watch(topic, handler);
    end,
    
    ---订阅一个系统消息(跨模块)
    ---@param topic integer
    ---@param handler fun(publisher:integer, mask:integer, ...):void
    ---@return boolean
    subscribe = function(topic, handler)
        return private.subscribe(topic, handler);
    end,
};

----------------------------------------------------------------------------
---封装 RPC 模块
----------------------------------------------------------------------------

local _rpc = private.rpc;

luaos.rpc = {
    ---注册一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@param func function
    ---@return boolean
    register = function(name, func)
        return _rpc.register(name, func);
    end,
    
    ---取消一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@return boolean
    unregister = function(name)
        return _rpc.cancel(name);
    end,
    
    ---同步执行一个 RPC 函数(当前进程有效)
    ---@param name string
    ---@param callback [function]
    ---@return boolean,...
    call = function(name, callback, ...)
        return _rpc.call(name, callback, ...);
    end,
};

----------------------------------------------------------------------------
---封装 SSL/TLS 模块
----------------------------------------------------------------------------

local _ssl = private.ssl;

luaos.ssl = {
    ---创建一个 SSL context
    ---@param cert string
    ---@param key string
    ---@param pwd string
    ---@return userdata
    context = function(cert, key, pwd)
        if not _ssl then
            return nil;
        end
        return _ssl.context(cert, key, pwd);
    end
};

----------------------------------------------------------------------------
---封装 nginx/http 模块
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
            return nil, "nginx is not found";
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
---封装集群模块
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
            return nil, "master is not found";
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
            return nil, "proxy is not found";
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
---封装全局存储模块
----------------------------------------------------------------------------

local pack, storage = luaos.conv, private.storage;
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
---封装随机数模块
----------------------------------------------------------------------------

local random;
ok, random = pcall(require, "luaos.random");
if ok then
    ---创建一个 32 位随机数
    ---@retrun integer
    local random32 = random.random32_create();
    luaos.random32 = function(...)
        random32:random(...);
    end
    
    ---创建一个 64 位随机数
    ---@retrun integer
    local random64 = random.random64_create();
    luaos.random64 = function(...)
        random64:random(...);
    end
else
    trace("module random is not installed");
end

----------------------------------------------------------------------------
---封装消息反应堆模块
----------------------------------------------------------------------------

local class_pump;
ok, class_pump = pcall(require, "luaos.pump");
if ok then
    local pump_message = class_pump();
    
    ---创建一个消息反应堆
    luaos.pump_message = function()
        return class_pump();
    end
    
    ---注册一个消息回调(仅当前模块)
    ---@param name integer|string
    ---@param handler fun(...):void
    ---@return function
    luaos.register = function(name, handler, priority)
        return pump_message:register(name, handler, priority);
    end
    
    ---取消一个消息回调(仅当前模块)
    ---@param name integer|string
    ---@param handler fun(...):void
    luaos.unregister = function(name, handler)
        return pump_message:unregister(name, handler);
    end
    
    ---派发一个回调消息(仅当前模块)
    ---@param name integer|string
    ---@return integer
    luaos.dispatch = function(name, ...)
        return pump_message:dispatch(name, ...);
    end
else
    trace("module pump-message is not installed");
end

----------------------------------------------------------------------------
---加载其他内置扩展模块
----------------------------------------------------------------------------

ok, luaos.bind = pcall(require, "luaos.bind");
if not ok then
    trace("module bind is not installed");
end

ok, luaos.try = pcall(require, "luaos.try");
if not ok then
    trace("module try is not installed");
end

ok, luaos.conv = pcall(require, "luaos.conv");
if not ok then
    trace("module conv is not installed");
end

ok, luaos.curl = pcall(require, "luaos.curl");
if not ok then
    trace("module curl is not installed");
end

ok, luaos.odbc = pcall(require, "luaos.odbc");
if not ok then
    trace("module odbc is not installed");
end

ok, luaos.class = pcall(require, "luaos.classy");
if not ok then
    trace("module classy is not installed");
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

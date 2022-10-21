

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

local success, bigint;
success, bigint = pcall(require, "bigint");
if not success then
	trace("bigint module is not installed");
else
	---创建一个大整数
	---@param v integer
	math.bigint = function(v)
		return bigint.new(v);
	end
end

----------------------------------------------------------------------------

---去掉字符串前后空格
---@param str string
---@retrun string
string.trim = function(str)
	return (string.gsub(str, "^%s*(.-)%s*$", "%1")) 
end

---将指定的字符串以指定的分隔符拆分
---@param str string
---@param seq string
---@retrun table
string.split = function(str, seq)
    if str == nil  or type(str) ~= "string" then
        return nil
    end
    local r, insert = {}, table.insert
    for word in string.gmatch(str,"[^{"..seq.."}*]+") do
        insert(r, word)
    end
    return r
end

----------------------------------------------------------------------------

local private = {
	storage = {
		set     = storage.set,
		get     = storage.get,
		erase   = storage.erase,
		clear   = storage.clear,
	},
	socket      = io.socket,
	deadline    = os.deadline,
	files       = os.files,
	timingwheel = os.timingwheel,
	typename    = os.typename,
	id          = os.id,
	execute     = os.execute,
	exit        = os.exit,
	wait        = os.wait,
	stopped     = os.stopped,
	publish     = os.publish,
	subscribe   = os.subscribe,
}

storage         = nil; --disused
io.socket       = nil; --disused
os.deadline     = nil; --disused
os.files        = nil; --disused
os.timingwheel  = nil; --disused
os.typename     = nil; --disused
os.id           = nil; --disused
os.execute      = nil; --disused
os.exit         = nil; --disused
os.wait         = nil; --disused
os.stopped      = nil; --disused
os.publish      = nil; --disused
os.subscribe    = nil; --disused

----------------------------------------------------------------------------

---@class luaos
local luaos = {
	read = 1, write = 2,
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
	
	---发布一个系统消息
	---@param topic integer
	---@param mask integer
	---@param receiver integer
	---@return integer
    publish = function(topic, mask, receiver, ...)
		return private.publish(topic, mask, receiver, ...);
	end,
	
	---取消一个消息订阅
	---@param id integer
    cancel = function(id)
		return private.cancel(id);
	end,
	
	---订阅一个系统消息
	---@param topic integer
	---@param handler fun(publisher:integer, ...):void
	---@return integer
    subscribe = function(topic, handler)
		return private.subscribe(topic, handler);
	end,
};

local random;
success, random = pcall(require, "luaos.random");
if not success then
	trace("random module is not installed");
else
    local random32 = random.random32_create();
	---创建一个 32 位随机数
	---@retrun integer
    luaos.random32 = function(...)
        random32:random(...);
    end
    
	---创建一个 64 位随机数
	---@retrun integer
    local random64 = random.random64_create();
    luaos.random64 = function(...)
        random64:random(...);
    end
end

local function on_error(err)
	error(debug.traceback(err))
end

---以保护模式运行函数
---@param func function
---@param ... any
---@return boolean,string
function luaos.pcall(func, ...)
	return xpcall(func, on_error, ...)
end

success, luaos.bind  = pcall(require, "luaos.bind");
if not success then
	trace("bind module is not installed");
end

success, luaos.conv  = pcall(require, "luaos.conv");
if not success then
	trace("conv module is not installed");
end

success, luaos.curl  = pcall(require, "luaos.curl");
if not success then
	trace("curl module is not installed");
end

success, luaos.heap  = pcall(require, "luaos.heap");
if not success then
	trace("heap module is not installed");
end

success, luaos.try   = pcall(require, "luaos.try");
if not success then
	trace("try module is not installed");
end

success, luaos.class = pcall(require, "luaos.classy");
if not success then
	trace("classy module is not installed");
end

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

local sandbox = {__newindex = function(...)
	throw("for backward compatibility, please do not modify luaos");
end};

return setmetatable(luaos, sandbox);

----------------------------------------------------------------------------

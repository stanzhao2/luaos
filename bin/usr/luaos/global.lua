

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

local string_split = string.split;

---去掉字符串前后空格
---@param str string
---@retrun string
string.trim = function(str)
    return (string.gsub(str, "^%s*(.-)%s*$", "%1")) ;
end

---按照指定分隔符拆分字符串
---@param str string
---@param delimiter string
---@retrun table
string.split = function(str, delimiter)
    return string_split(str, delimiter);
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

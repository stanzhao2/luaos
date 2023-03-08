

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

local pack = require("msgpack")
local conv = require("openssl")
local curl = require("curl")
local json = require("rapidjson")

----------------------------------------------------------------------------
---@class url
local x_url = {};
--- url编码字符串
---
--- Returns 解码后的字符串
---
--- 'str' 待编码的字符串
---@param str string
---@return string
function x_url.escape(str) end

--- url解码字符串
---
--- Returns 解码后的字符串
---
--- 'str' 待解码的字符串
---@param str string
---@return string
function x_url.unescape(str) end

----------------------------------------------------------------------------
---@class json
local x_json = {};
--- 将json字符串反序列成LUA的变量
---
--- Returns LUA的变量
---
--- 'str' 是一个json标准的字符串
---@param str string
---@return any
function x_json.decode(str) end

--- 将LUA的变量序列化成字符串
---
--- Returns json字符串
---
--- 'value' 是一个lua的一个变量，可以是以下类型
--- **`number`**
--- **`boolean`**
--- **`table`**
---@param value any
---@return string
function x_json.encode(value) end

----------------------------------------------------------------------------
---@class pack
local x_pack = {};
--- 将json字符串反序列成LUA的变量
---
--- Returns LUA的变量
---
--- 'str' 是一个json标准的字符串
---@param str string
---@return any
function x_pack.decode(str) end

function x_pack.decode_one(str, offset) end

function x_pack.decode_limit(str, limit, offset) end

--- 将LUA的变量序列化成字符串
---
--- Returns json字符串
---
--- 'value' 是一个lua的一个变量，可以是以下类型
--- **`number`**
--- **`boolean`**
--- **`table`**
---@param value any
---@return string
function x_pack.encode(value, ...) end

----------------------------------------------------------------------------
---@class xor
local x_xor = {};

--- 字符串异或加解密
---
--- Returns 编码后的Lua值
---
--- 'str' 源字符串
--- 'key' 密钥字符串
---@param str string
---@param key string
---@return string
function x_xor.convert(str, key) end

----------------------------------------------------------------------------
---@class hash
local x_hash = {};

---计算字符串的crc32值
---
--- Returns crc32值
---
--- 'data' 计算的数据
---@param data string
---@return number
function x_hash.crc32(data) end

---计算字符串的sha1值
---
--- Returns sha1值,如果raw缺省或者false，则返回sha1的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'raw' 是否原值
---@overload fun(data):string
---@param data string
---@param raw boolean
---@return string
function x_hash.sha1(data, raw) end

---计算字符串的签名hmac_sha1值
---
--- Returns hmac_sha1值,如果raw缺省或者false，则返回hmac_sha1的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'key' 密钥
---
--- 'raw' 是否原值
---@overload fun(data, key):string
---@param data string
---@param key string
---@param raw boolean
---@return string
function x_hash.hmac_sha1(data, key, raw) end

---计算字符串的sha224值
---
--- Returns sha224值,如果raw缺省或者false，则返回sha224的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'raw' 是否原值
---@overload fun(data):string
---@param data string
---@param raw boolean
---@return string
function x_hash.sha224(data, raw) end

---计算字符串的签名hmac_sha224值
---
--- Returns hmac_sha224值,如果raw缺省或者false，则返回hmac_sha224的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'key' 密钥
---
--- 'raw' 是否原值
---@overload fun(data, key):string
---@param data string
---@param key string
---@param raw boolean
---@return string
function x_hash.hmac_sha224(data, key, raw) end

---计算字符串的sha256值
---
--- Returns sha256值,如果raw缺省或者false，则返回sha256的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'raw' 是否原值
---@overload fun(data):string
---@param data string
---@param raw boolean
---@return string
function x_hash.sha256(data, raw) end

---计算字符串的签名hmac_sha256值
---
--- Returns hmac_sha256值,如果raw缺省或者false，则返回hmac_sha256的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'key' 密钥
---
--- 'raw' 是否原值
---@overload fun(data, key):string
---@param data string
---@param key string
---@param raw boolean
---@return string
function x_hash.hmac_sha256(data, key, raw) end

---计算字符串的sha384值
---
--- Returns sha384值,如果raw缺省或者false，则返回sha384的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'raw' 是否原值
---@overload fun(data):string
---@param data string
---@param raw boolean
---@return string
function x_hash.sha384(data, raw) end

---计算字符串的签名hmac_sha384值
---
--- Returns hmac_sha384值,如果raw缺省或者false，则返回hmac_sha384的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'key' 密钥
---
--- 'raw' 是否原值
---@overload fun(data, key):string
---@param data string
---@param key string
---@param raw boolean
---@return string
function x_hash.hmac_sha384(data, key, raw) end

---计算字符串的sha512值
---
--- Returns sha512值,如果raw缺省或者false，则返回sha512的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'raw' 是否原值
---@overload fun(data):string
---@param data string
---@param raw boolean
---@return string
function x_hash.sha512(data, raw) end

---计算字符串的签名hmac_sha512值
---
--- Returns hmac_sha512值,如果raw缺省或者false，则返回hmac_sha512值的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'key' 密钥
---
--- 'raw' 是否原值
---@overload fun(data, key):string
---@param data string
---@param key string
---@param raw boolean
---@return string
function x_hash.hmac_sha512(data, key, raw) end

---计算字符串的md5值
---
--- Returns md5值,如果raw缺省或者false，则返回md5的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'raw' 是否原值
---@overload fun(data):string
---@param data string
---@param raw boolean
---@return string
function x_hash.md5(data, raw) end

---计算字符串的签名hmac_md5值
---
--- Returns hmac_md5值,如果raw缺省或者false，则返回hmac_md5的字符串，否则返回二进制值
---
--- 'data' 计算的数据
---
--- 'key' 密钥
---
--- 'raw' 是否原值
---@overload fun(data, key):string
---@param data string
---@param key string
---@param raw boolean
---@return string
function x_hash.hmac_md5(data, key, raw) end

----------------------------------------------------------------------------
---@class base64
local x_base64 = {};

---Base64编码
---
--- Returns 编码后的字符串
---
--- 'data' 编码的数据
---@param str string
---@return string
function x_base64.encode(data) end


---Base64解码
---
--- Returns 解码后的数据
---
--- 'str' 字符串
---@param str string
---@return string
function x_base64.decode(str) end

----------------------------------------------------------------------------
---@class aes
local x_aes = {};

---aes加密
---
--- Returns 加密后数据
---
--- 'data' 要加密的数据
---
--- 'key' 密钥
---@param data string
---@param key string
---@return string
function x_aes.encrypt(data, key) end


---aes解密
---
--- Returns 解密后数据
---
--- 'data' 要解密的数据
---
--- 'key' 密钥
---@param data string
---@param key string
---@return string
function x_aes.decrypt(data, key) end

----------------------------------------------------------------------------
---@class rsa
local x_rsa = {};

---RSA签名
---
--- Returns 签名串
---
--- 'data' 要签名的数据
---
--- 'key' 私钥
---@param data string
---@param key string
---@return string
function x_rsa.sign(data, key) end

---RSA签名验证
---
--- Returns 是否验证成功
---
--- 'src' 原始数据
---
--- 'sign' 签名数据
---
--- 'key' 私钥
---@param src  string
---@param sign string
---@param key  string
---@return boolean
function x_rsa.verify(src, sign, key) end

---RSA加密
---
--- Returns 加密后数据
---
--- 'data' 公钥/私钥
---
--- 'key' public"/"private
---
--- 'type' 私钥
---@param data  string
---@param key  string
---@return boolean
function x_rsa.encrypt(data, key) end

---RSA解密
---
--- Returns 要解密的数据
---
--- 'data' 公钥/私钥
---
--- 'key' public"/"private
---
---@param data  string
---@param key  string
---@return boolean
function x_rsa.decrypt(data, key) end


---@class conv
local x = {
    ---@type url
    url = curl,
    ---@type json
    json = json,
    ---@type pack
    pack = pack,
    ---@type xor
    xor = conv.xor,
    ---@type hash
    hash = conv.hash,
    ---@type base64
    base64 = conv.base64,
    ---@type aes
    aes = conv.aes,
    ---@type rsa
    rsa = conv.rsa,
}

----------------------------------------------------------------------------
--- 将json字符串反序列成LUA的变量
---
--- Returns LUA的变量
---
--- 'str' 是一个json标准的字符串
---@param str string
---@return any
function x.stov(str)
    return json.decode(str)
end

----------------------------------------------------------------------------
--- 将LUA的变量序列化成字符串
---
--- Returns json字符串
---
--- 'v' 是一个lua的一个变量，可以是以下类型
--- **`number`**
--- **`boolean`**
--- **`table`**
---@param v any
---@return string
function x.vtos(v)
    return json.encode(v)
end

----------------------------------------------------------------------------
--- 去掉字符串前后空格
---
--- Returns 去除前后空格的字符串
---
--- 'str' 原始需要被处理的字符串
---@param str string
---@return string
function x.trim(str)
    return (string.gsub(str, "^%s*(.-)%s*$", "%1"))
end


return x

----------------------------------------------------------------------------

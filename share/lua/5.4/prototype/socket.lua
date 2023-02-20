

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

---@class luaos_socket
local i_socket = {};

---创建本地 TCP 监听,成功时返回true,否则返回false或nil
---@param host string
---@param port integer
---@param handler fun(peer:socket):void
---@return boolean|nil
function i_socket:listen(host, port, handler) end;

---创建本地 UDP 绑定,成功时返回 true，否则返回 false 或 nil
---@param host string
---@param port integer
---@param handler fun(ec:integer, data:string):void
---@return boolean|nil
function i_socket:bind(host, port, handler) end;

---与远程主机建立连接,成功时返回true,否则返回false或nil
---@overload fun(host:string, port:integer, handler:fun(ec:integer):void):boolean
---@param host string
---@param port integer
---@param timeout integer
---@return boolean|nil
function i_socket:connect(host, port, timeout) end;

---关闭一个有效的 socket
function i_socket:close() end;

---判断一个 socket 是否是打开状态,是则返回 true，否则返回 false 或 nil
---@return boolean|nil
function i_socket:is_open() end;

---获取 socket 的编号,成功返回 id，否则返回nil
---@return integer|nil
function i_socket:id() end;

---关闭 TCP 协议的 nagle 算法,成功则返回 true，否则返回 false 或 nil
---@return boolean|nil
function i_socket:nodelay() end;

---获取 socket 当前可读取的字节数,成功返回可读取的字节数，否则返回 nil
---@return integer|nil
function i_socket:available() end;

---设置 socket 连接超时时间(keep-alive time),成功返回 true，否则返回 false 或 nil
---@param millisecond integer
---@return boolean|nil
function i_socket:timeout(millisecond) end;

---获取 socket 远端地址信息,成功返回 ip 和 端口，否则返回 nil
---@return string,integer
function i_socket:endpoint() end;

---设置 socket 接收回调函数,成功则返回 true，否则返回 false 或 nil
---@param handler fun(ec:integer, data:string):void
---@return boolean|nil
function i_socket:select(handler) end;

---对要发送的数据进行编码(内置算法),成功返回 true，否则返回 false 或 nil
---@param data string
---@param opcode integer
---@param encrypt boolean
---@param compress boolean
---@return string|nil
function i_socket:encode(data, opcode, encrypt, compress) end;

---发送数据,成功则返回发送的字节数，否则返回 nil
---@param data string
---@param asynchronous boolean|nil
---@return integer|nil
function i_socket:send(data, asynchronous) end;

---发送数据(只用于 UDP),成功则返回发送的字节数，否则返回 nil
---@param data string
---@param host string
---@param port integer
---@param asynchronous boolean|nil
---@return integer|nil
function i_socket:send_to(data, host, port, asynchronous) end;

---对收到的数据进行解码,成功则返回未解码的数据长度，否则返回 nil
---@param data string
---@param handler fun(data:string, opcode:integer):void
---@return integer|nil,reason
function i_socket:decode(data, handler) end;

---用阻塞的方式接收数据,成功则返回接收到的数据，否则返回 nil
---@param size integer
---@return string|nil
function i_socket:receive(size) end;

---用阻塞的方式接收数据(只用于 UDP),成功则返回接收到的数据，远端地址及端口，否则返回 nil
---@param size integer
---@return string,string,integer|nil
function i_socket:receive_from(size) end;

---开启 socket SSL 功能
---@param  ctx ssl-context
---@param  certfile string|nil
---@return boolean
function i_socket:sslv23(ctx, certfile) end;

---SSL 握手
---@overload fun(handler:fun(ec:integer):void):boolean
---@return boolean
function i_socket:handshake() end

----------------------------------------------------------------------------

return i_socket;

----------------------------------------------------------------------------

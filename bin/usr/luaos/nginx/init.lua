

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

local luaos  = require("luaos")
local gzip   = require("gzip")
local parser = require("luaos.parser")

local pcall = pcall;
if _DEBUG then
    pcall = luaos.pcall
end

local bind   = luaos.bind;
local try    = luaos.try;
local conv   = luaos.conv;
local socket = luaos.socket;

local http_status_text = {
    [100] = "Continue",
    [101] = "Switching Protocols",
    [200] = "OK",
    [201] = "Created",
    [202] = "Accepted",
    [203] = "Non-Authoritative Information",
    [204] = "No Content",
    [205] = "Reset Content",
    [206] = "Partial Content",
    [300] = "Multiple Choices",
    [301] = "Moved Permanently",
    [302] = "Found",
    [303] = "See Other",
    [304] = "Not Modified",
    [305] = "Use Proxy",
    [307] = "Temporary Redirect",
    [400] = "Bad Request",
    [401] = "Unauthorized",
    [402] = "Payment Required",
    [403] = "Forbidden",
    [404] = "File Not Found",
    [405] = "Method Not Allowed",
    [406] = "Not Acceptable",
    [407] = "Proxy Authentication Required",
    [408] = "Request Time-out",
    [409] = "Conflict",
    [410] = "Gone",
    [411] = "Length Required",
    [412] = "Precondition Failed",
    [413] = "Request Entity Too Large",
    [414] = "Request-URI Too Large",
    [415] = "Unsupported Media Type",
    [416] = "Requested range not satisfiable",
    [417] = "Expectation Failed",
    [500] = "Internal Server Error",
    [501] = "Not Implemented",
    [502] = "Bad Gateway",
    [503] = "Service Unavailable",
    [504] = "Gateway Time-out",
    [505] = "HTTP Version not supported",
}

local http_mime_type = {
    cod   = "image/cis-cod",
    ras   = "image/cmu-raster",
    fif   = "image/fif",
    gif   = "image/gif",
    ief   = "image/ief",
    jpeg  = "image/jpeg",
    jpg   = "image/jpeg",
    jpe   = "image/jpeg",
    png   = "image/png",
    tiff  = "image/tiff",
    tif   = "image/tiff",
    mcf   = "image/vasa",
    wbmp  = "image/vnd.wap.wbmp",
    fh4   = "image/x-freehand",
    fh5   = "image/x-freehand",
    fhc   = "image/x-freehand",
    pnm   = "image/x-portable-anymap",
    pbm   = "image/x-portable-bitmap",
    pgm   = "image/x-portable-graymap",
    ppm   = "image/x-portable-pixmap",
    rgb   = "image/x-rgb",
    ico   = "image/x-icon",
    xwd   = "image/x-windowdump",
    xbm   = "image/x-xbitmap",
    xpm   = "image/x-xpixmap",
    js    = "text/javascript",
    css   = "text/css",
    htm   = "text/html",
    html  = "text/html",
    shtml = "text/html",
    txt   = "text/plain",
    pdf   = "application/pdf",
    xml   = "application/xml",
    json  = "application/json",
}

local gzip_encoding = {
    js    = true,
    css   = true,
    htm   = true,
    html  = true,
    shtml = true,
    txt   = true,
    xml   = true,
    json  = true,
}

local _WWWROOT = "nginx"
local _STATE_OK                 = 200
local _STATE_LOCATION           = 301
local _STATE_NOT_FOUND          = 404
local _STATE_ERROR              = 502

local _STATE_OK_TEXT            = "OK"
local _STATE_FAILED_TEXT        = "Failed"

local _HEADER_CONNECTION        = "Connection"
local _HEADER_ACCEPT_ENCODING   = "Accept-Encoding"
local _HEADER_CONTENT_ENCODING  = "Content-Encoding"
local _HEADER_PRAGMA            = "Pragma"
local _HEADER_SERVER            = "Server"
local _HEADER_UPGRADE           = "Upgrade"
local _HEADER_LOCATION          = "Location"
local _HEADER_CACHE_CONTROL     = "Cache-Control"
local _HEADER_CONTENT_TYPE      = "Content-Type"
local _HEADER_CONTENT_LENGTH    = "Content-Length"
local _HEADER_WEBSOCKET_KEY        = "Sec-WebSocket-Key"
local _HEADER_WEBSOCKET_ACCEPT     = "Sec-WebSocket-Accept"
local _HEADER_WEBSOCKET_EXTENSIONS = "Sec-WebSocket-Extensions"

local table_insert  = table.insert
local table_concat  = table.concat
local string_format = string.format
local string_match  = string.match
local string_trim   = string.trim
local string_split  = string.split
local string_gsub   = string.gsub
local string_sub    = string.sub
local string_lower  = string.lower
local string_pack   = string.pack
local string_unpack = string.unpack

local function default_headers()
    local headers = {
        [_HEADER_SERVER]        = "LuaOS-nginx",
        [_HEADER_CONNECTION]    = "Close",
        [_HEADER_CACHE_CONTROL] = "max-age=0",
        [_HEADER_CONTENT_TYPE]  = "text/html;charset=UTF-8"
    }
    function headers:write(data)
        table_insert(self, data)
    end
    function headers:header(key, value)
        self[key] = value
    end
    function headers:location(url)
        self[_HEADER_LOCATION] = url
    end
    return headers
end

local function http_encode_data(headers, data)
    local encoding = headers[_HEADER_CONTENT_ENCODING]
    if encoding == "gzip" then
        data = gzip.deflate(data, true)
    elseif encoding == "deflate" then
        data = gzip.deflate(data, false)
    end
    return data
end

local function send_message(peer, data)
    peer:send(data, true);
end

local function on_http_error(peer, headers, code)
    local cache, head = {}
    if not code then
        code = _STATE_ERROR
    end
    
    local text = http_status_text[code]
    head = string_format("HTTP/1.1 %d %s\r\n", code, text)
    table_insert(cache, head);
    
    text = "<b><i>" .. text .. "</i></b>"
    head = string_format(_HEADER_CONTENT_LENGTH .. ": %d\r\n", #text)
    table_insert(cache, head);
    
    local headers = default_headers()
    for k, v in pairs(headers) do
        if type(k) == "string" then
            if type(v) ~= "function" then
                table_insert(cache, string_format("%s: %s\r\n", k, v))
            end
        end
    end
    
    table_insert(cache, "\r\n");
    send_message(peer, table_concat(cache))
    send_message(peer, text)
    if "close" == headers[_HEADER_CONNECTION] then
        peer:close()
    end
    return _STATE_FAILED_TEXT
end

local function on_http_success(peer, headers, code)
    local cache, head = {}
    if not code then
        code = _STATE_OK
    end
    
    local text = http_status_text[code]
    local location = headers[_HEADER_LOCATION]
    if location then
        code = _STATE_LOCATION
        text = http_status_text[code]
    end
    
    head = string_format("HTTP/1.1 %d %s\r\n", code, text)
    table_insert(cache, head);
    
    local data
    if #headers > 0 then
        data = table_concat(headers)
        data = http_encode_data(headers, data)
    end
    
    local has_body = false
    if code == 200 and data and #data > 0 then
        has_body = true
    end
    
    if has_body then
        head = string_format(_HEADER_CONTENT_LENGTH .. ": %d\r\n", #data)
        table_insert(cache, head);
    end
    
    for k, v in pairs(headers) do
        if type(k) == "string" then
            if type(v) ~= "function" then
                table_insert(cache, string_format("%s: %s\r\n", k, v))
            end
        end
    end
    
    table_insert(cache, "\r\n");
    send_message(peer, table_concat(cache))
    if has_body then
        send_message(peer, data)
    end
    if "close" == headers[_HEADER_CONNECTION] then
        peer:close()
    end
    return _STATE_OK_TEXT
end

local function on_http_download(peer, headers, filename, ext)
    filename = _WWWROOT .. filename
    filename = string_gsub(filename, '%.', '/')
    filename = string_sub(filename, 1, #filename - #ext - 1)
    filename = "./lua/" .. filename .. '.' .. ext
    
    local code = _STATE_NOT_FOUND
    local mime = http_mime_type[ext]
    if not mime then
        return on_http_error(peer, headers, code)
    end
    
    local fs = io.open(filename, "rb")
    if not fs then
        return on_http_error(peer, headers, code)
    end
    
    local ok, data = pcall(fs.read, fs, "a")
    fs:close()
    if not ok or not data or #data == 0 then
        return on_http_error(peer, headers, code)
    end
    
    if not gzip_encoding[ext] then
        headers[_HEADER_CONTENT_ENCODING] = nil
    end
    
    headers[_HEADER_CONTENT_TYPE] = mime
    table_insert(headers, data)
    return on_http_success(peer, headers)
end

----------------------------------------------------------------------------

local function parse_url_params(params)
    if not params then
        return nil
    end
    local result
    local argvs = string_split(params, '&')
    for i = 1, #argvs do
        local x = string_split(argvs[i], '=')
        if not result then
            result = {}
        end
        local key;
        if x[1] then
            key = string_trim(x[1])
        end
        local value
        if x[2] then
            value = string_trim(x[2])
        end
        if key and value then
            result[key] = value
        end
    end
    return result
end

local function url_to_filename(url)
    local uri    = string_split(url, '?');
    local params = parse_url_params(uri[2]);
    local path   = string_split(uri[1], '/');
    
    local depth = #path;
    if depth == 0 then
        depth = 1;
        table_insert(path, "index");
    else
        local cnt = 0;
        for i = 1, depth do
            if path[i] ~= ".." then
                cnt = cnt + 1;
            else
                cnt = cnt - 1;
            end
            if cnt < 0 then
                throw("Path depth attack");
            end
        end
    end
    
    local filename = ""
    for i = 1, depth do
        filename = filename .. "." .. path[i];
    end
    return filename, path, params;
end

local function on_http_request(peer, request)
    local headers = default_headers()
    local rheader = request:headers()
    
    local connection = rheader[_HEADER_CONNECTION]
    if connection then
        headers[_HEADER_CONNECTION] = rheader[_HEADER_CONNECTION]
    end
    
    local encoding = rheader[_HEADER_ACCEPT_ENCODING]
    if encoding then
        if string_match(encoding, "gzip") then
            headers[_HEADER_CONTENT_ENCODING] = "gzip"
        elseif string_match(encoding, "deflate") then
            headers[_HEADER_CONTENT_ENCODING] = "deflate"
        end
    end
    
    local url = conv.url.unescape(request:url())
    local filename, path, params = url_to_filename(url)
    
    local others = path[#path]
    others = string_split(others, '.')
    
    ---读取静态文件
    if #others > 1 then
        local ext = others[#others]
        return on_http_download(peer, headers, filename, ext)
    end
    
    ---加载脚本文件
    local ok, script = pcall(require, _WWWROOT .. filename)
    if not ok then
        return on_http_error(peer, headers, _STATE_ERROR)
    end
    
    if type(script) ~= "table" then
        return on_http_error(peer, headers, _STATE_ERROR)
    end
    
    if type(script.on_request) ~= "function" then
        return on_http_error(peer, headers, _STATE_ERROR)
    end
    
    ---运行脚本文件
    local ok, result = pcall(script.on_request, request, headers, params)
    if not ok then
        return on_http_error(peer, headers, _STATE_ERROR)
    end
    
    return on_http_success(peer, headers)
end

----------------------------------------------------------------------------
--[=[
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
--]=]

local op_code = {
    ["frame" ] =  0x00,
    ["text"  ] =  0x01,
    ["binary"] =  0x02,
    ["close" ] =  0x08,
    ["ping"  ] =  0x09,
    ["pong"  ] =  0x0A,
    [ 0x00   ] = "frame",
    [ 0x01   ] = "text",
    [ 0x02   ] = "binary",
    [ 0x08   ] = "close",
    [ 0x09   ] = "ping",
    [ 0x0A   ] = "pong",
};

local ws_reason = {
    [1000] = "Normal Closure",
    [1001] = "Server actively disconnects",
    [1002] = "Websocket protocol error",
    [1008] = "The frame not include mask",
    [1009] = "The length of packet is too large",
};

local _WS_FRAME_SIZE = 0xffff;
local _WS_MAX_PACKET <const> = 64 * 1024 * 1024
local _WS_TRUST_TIMEOUT = 300000;
local _WS_UNTRUST_TIMEOUT = 5000;

local function ws_insert(cache, size, length)
    table_insert(cache, string_pack("B", size));
    if not length then
        return;
    end
    if size == 126 then
        table_insert(cache, string_pack(">I2", length));
        return;
    end
    if size == 127 then
        table_insert(cache, string_pack(">I8", length));
        return;
    end
end

local function ws_frame(data, fin, op, deflate, first)    
    local flag = fin;
    if first and deflate then
        flag = flag | 0x40;
    end
    
    local size = #data;
    local cache = {};
    table_insert(cache, string_pack("B", op | flag))
    
    if size < 126 then
        ws_insert(cache, size, nil);
    elseif size <= 0xffff then
        ws_insert(cache, 126, size);
    else
        ws_insert(cache, 127, size);
    end
    
    table_insert(cache, string_pack("c" .. size, data));
    return table_concat(cache);
end

local function ws_opcode(data)
    local opcode = op_code.binary;
    if utf8.check(data) then
        opcode = op_code.text;
    end
    return opcode;
end

local function ws_encode(data, op, deflate)
    if op == nil then
        op = ws_opcode(data);
    end
    
    assert(op == op_code.text or op == op_code.binary);
    
    local fsize = _WS_FRAME_SIZE;
    if deflate then
        data = gzip.deflate(data, false);
    end

    local packet    = {};
    local length    = #data;
    local pos       = 1;
    local first     = true;
    local splite    = false;
    
    while length > 0 do
        local size = length;
        if size > fsize then
            size = fsize;
        end
        
        if size < length then
            splite = true;
        end
        
        local fin = 0;
        length = length - size;
        if length == 0 then
            fin = 0x80;
        end
        
        local frame = data;
        if splite then
            frame = string_unpack("c" .. size, data, pos);
        end
        
        frame = ws_frame(frame, fin, op, deflate, first);
        table_insert(packet, frame);
        
        pos = pos + size;
        first = false;
    end
    
    return table_concat(packet);
end

local function ws_close(peer, code)
    local len = 0;
    local reason = ws_reason[code];
    if reason then
        len = #reason;
    end
    
    local message = {};
    table_insert(message, string_pack("B", op_code.close | 0x80));
    table_insert(message, string_pack("B", len + 2));
    table_insert(message, string_pack(">I2", code));
    
    if reason then
        table_insert(message, string_pack("c" .. len, reason));
    end
    
    if code ~= 1000 and reason then
        error(reason);
    end
    
    send_message(peer, table_concat(message));
    peer:close();
end

local function ws_pong(peer)
    local r = {};
    table_insert(r, string_pack("B", op_code.pong | 0x80));
    table_insert(r, string_pack("B", 0));
    send_message(peer, table_concat(r));
end

local function wrap_ws_socket(peer, deflate)
    local _ws_socket = {
        peer = peer, deflate = deflate
    };

    function _ws_socket:endpoint()
        if self.peer then
            return self.peer:endpoint();
        end
    end

    function _ws_socket:id()
        if self.peer then
            return self.peer:id();
        end
    end

    function _ws_socket:close()
        if self.peer then
            ws_close(self.peer, 1001);
            self.peer = nil;
        end
    end
    
    function _ws_socket:send(data, opcode)
        if self.peer then
            local deflate = self.deflate;
            data = ws_encode(data, opcode, deflate);
            send_message(self.peer, data);
        end
    end

    return _ws_socket;
end

local function on_ws_request(session, fin, data, opcode)
    local peer = session.peer
    
    --客户端主动关闭连接
    if opcode == op_code.close then
        ws_close(peer, 1000);
        return;
    end
    
    --客户端发送来的心跳消息
    if opcode == op_code.ping then
        ws_pong(peer);
        return;
    end
    
    --如果所有数据帧未收全
    if not fin then
        if not session.packet then
            session.pktlen = 0;
            session.packet = {};
        end
        
        table_insert(session.packet, data);
        session.pktlen = session.pktlen + #data;
        
        --如果收到的数据长度大于限制则关闭连接
        if session.pktlen > _WS_MAX_PACKET then
            ws_close(peer, 1009);
        end
        return
    end
    
    if session.packet then
        data = table_concat(session.packet) .. data;
        session.pktlen = nil;
        session.packet = nil;
    end
    
    if session.deflate then --deflate
        data = gzip.inflate(data);
        session.deflate = false;
    end
    
    if opcode == op_code.text then
        if not utf8.check(data) then
            ws_close(peer, 1002);
            return;
        end
    end
    
    local handler = session.ws_handler;
    if handler then
        local ws_peer = session.ws_peer;
        handler.on_receive(ws_peer, 0, data, opcode);
    end
end

local function on_ws_receive(session, data)
    local peer = session.peer;
    if session.cache and #session.cache > 0 then
        data = table_concat(session.cache) .. data;
        session.cache = nil;
    end
    
    local offset = 1;
    local cache_size = #data;
    while cache_size > 1 do
        local pos = offset;
        local v1, v2 = string_unpack("I1I1", data, pos);
        
        local fin = (v1 & 0x80) ~= 0;
        local deflate = (v1 & 0x40) ~= 0;
        
        --如果设置压缩标记,则只能在第一帧设置
        if deflate then
            if session.deflate then
                ws_close(peer, 1002);
                return;
            end
            --标记当前帧数据需要解压缩
            session.deflate = deflate;
        end
        
        --rsv2 unused, must be zero
        local rsv2 = (v1 & 0x20) ~= 0;
        if rsv2 then
            ws_close(peer, 1002);
            return;
        end
        
        --rsv2 unused, must be zero
        local rsv3 = (v1 & 0x10) ~= 0;
        if rsv3 then
            ws_close(peer, 1002);
            return;
        end
        
        local opcode = (v1 & 0x0f);
        
        --客户端发来的数据包必须设置 mask 位
        local mask = (v2 & 0x80) ~= 0;
        if not mask then
            ws_close(peer, 1002);
            return;
        end
        
        local payload_len = (v2 & 0x7f);     
        pos = pos + 2;
        
        if payload_len == 126 then
            if cache_size < pos + 1 then
                break;
            end
            payload_len = string_unpack(">I2", data, pos);
            pos = pos + 2;
        elseif payload_len == 127 then
            if cache_size < pos + 7 then
                break;
            end
            payload_len = string_unpack(">I8", data, pos);
            pos = pos + 8;
        end
        
        --如果数据帧长度大于限制则关闭连接
        if payload_len > _WS_MAX_PACKET then
            ws_close(peer, 1009);
            return;
        end
        
        local masking_key = nil;
        if mask then
            if cache_size < pos + 3 then
                break;
            end
            masking_key = string_unpack("c4", data, pos);
            pos = pos + 4;
        end
        
        local frame_size = payload_len + pos - 1;
        if frame_size > cache_size then
            break;
        end
        
        local packet = string_unpack("c" .. payload_len, data, pos);
        if masking_key then
            packet = conv.xor.convert(packet, masking_key);
        end
        
        on_ws_request(session, fin, packet, opcode);
        
        pos = pos + payload_len;
        cache_size = cache_size - frame_size;
        offset = offset + frame_size;
    end
    
    if cache_size > 0 then
        if not session.cache then
            session.cache = {};
        end
        data = string_unpack("c" .. cache_size, data, offset);
        table_insert(session.cache, data);
    end
end

local function on_ws_accept(session, request)
    local peer = session.peer;
    local headers = default_headers();
    local rheader = request:headers();
    
    local key = rheader[_HEADER_WEBSOCKET_KEY]
    if not key then
        on_http_error(peer, _STATE_ERROR);
        return;
    end
    
    local url = conv.url.unescape(request:url());
    local filename, path, params = url_to_filename(url);
    
    local others = path[#path];
    others = string_split(others, '.');
    
    if #others > 1 then
        on_http_error(peer, _STATE_ERROR);
        return;
    end
    
    ---加载脚本文件
    local ok, script = pcall(require, _WWWROOT .. filename);
    if not ok then
        on_http_error(peer, _STATE_ERROR);
        return;
    end
    
    if type(script) ~= "table" then
        on_http_error(peer, _STATE_ERROR);
        return;
    end
    
    if type(script.on_receive) ~= "function" then
        on_http_error(peer, _STATE_ERROR);
        return;
    end
    
    session.ws_peer = wrap_ws_socket(peer, deflate);
    
    if type(script.on_handshake) == "function" then
        local ok, result = pcall(script.on_handshake, ws_peer, request, params);
        if not ok or not result then
            on_http_error(peer, _STATE_ERROR);
            return;
        end
    end
    
    peer:timeout(_WS_TRUST_TIMEOUT);
    session.ws_handler = script;
    
    ---如果请求者需要跨域连接
    local origin = rheader["Origin"];
    if origin then
        headers["Access-Control-Allow-Credentials"] = "true";
        headers["Access-Control-Allow-Origin"] = origin;
    end
    
    local deflate = false;
    local externs = rheader[_HEADER_WEBSOCKET_EXTENSIONS];
    
    ---如果开启了数据压缩功能
    if externs then
        local x = string_match(externs, "permessage%-deflate");
        if x then
            deflate = true;
            headers[_HEADER_WEBSOCKET_EXTENSIONS] = "permessage-deflate; client_no_context_takeover; server_max_window_bits=15";
        end
    end
    
    key = key .. "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    local hash = conv.hash.sha1(key, true);
    key = conv.base64.encode(hash);
    
    headers[_HEADER_CONNECTION]    = "Upgrade";
    headers[_HEADER_UPGRADE]       = "websocket";
    headers[_HEADER_PRAGMA]        = "no-cache";
    headers[_HEADER_CACHE_CONTROL] = "max-age=0";
    headers[_HEADER_WEBSOCKET_ACCEPT] = key;
    on_http_success(peer, headers, 101);
    
    local handler = session.ws_handler;
    if handler and type(handler.on_accept) == "function" then
        handler.on_accept(session.ws_peer, request, params);
    end
end

----------------------------------------------------------------------------

local sessions = {}

local _ws_upgrade = false;

local function on_http_callback(session, request)
    local peer = session.peer;
    local method = request:method();
    if method ~= "GET" and method ~= "POST" then
        peer:close();
        return false;
    end
    
    local upgrade = request:is_upgrade()
    if upgrade then
        if not _ws_upgrade then
            peer:close();
        else
            session.upgrade = true;
            pcall(on_ws_accept, session, request);
        end
        return;
    end
    
    peer:timeout(_WS_TRUST_TIMEOUT);
    local from = peer:endpoint();
    local ok, result = pcall(on_http_request, peer, request);
    if not ok then
        peer:close();
        return;
    end
    local url = request:url();
    if url then
        local method = request:method();
        print(method .. " " .. url .. " " .. result .. " from: " .. from);
    end
end

local function on_socket_error(peer, ec)
    local fd = peer:id();
    
    if fd > 0 then
        local session = sessions[fd];
        if session and session.upgrade then
            if session.ws_handler then
                local handler = session.ws_handler;
                pcall(handler.on_receive, peer, ec);
            end
        end
        sessions[fd] = nil;
    end
    
    if peer:is_open() then
        peer:close();
    end
end

local function on_socket_receive(peer, data)
    local session = sessions[peer:id()]    
    if not session then
        return;
    end
    
    if session.upgrade then
        on_ws_receive(session, data);
        return;
    end
    
    local rsize = #data
    local request = session.parser
    while rsize > 0 do
        local errno, size = request:parse(data, bind(on_http_callback, session))
        if errno > 0 then
            peer:close()
            break
        end
        if rsize >= size then
            rsize = rsize - size
        else
            break
        end
    end
end

local function on_receive_handler(peer, ec, data)
    if ec > 0 then
        on_socket_error(peer, ec);
        return;
    end
    if not pcall(on_socket_receive, peer, data) then
        peer:close()
    end
end

local function on_ssl_handshake(peer, ec)
    if ec > 0 then
        peer:close();
        return;
    end
    
    local fd      = peer:id()
    local from    = peer:endpoint()
    local session = sessions[fd]
    
    if not session then
        session = {}
        session.peer   = peer
        session.parser = parser(false)
        sessions[fd]   = session
        peer:select(luaos.read, bind(on_receive_handler, peer))
    end
end

local function on_socket_accept(ctx, peer)
    if ctx then
        peer:sslv23(ctx);
    end
    peer:timeout(_WS_UNTRUST_TIMEOUT);
    peer:handshake(bind(on_ssl_handshake, peer));
end

----------------------------------------------------------------------------

local nginx = {}

function nginx.upgrade()
    _ws_upgrade = true;
end

function nginx.stop()
    if nginx.acceptor then
        nginx.acceptor:close();
        nginx.acceptor = nil;
    end
    for k, v in pairs(sessions) do
        v.peer:close();
        v.peer = nil;
        v.parser = nil;
    end
    sessions = {};
end

function nginx.start(host, port, wwwroot, ctx)
    if nginx.acceptor then
        return false
    end
    
    if wwwroot then
        _WWWROOT = wwwroot
        _WWWROOT = string_gsub(_WWWROOT, '/', '.')
        _WWWROOT = string_gsub(_WWWROOT, '\\', '.')
    end
    
    host = host or "0.0.0.0";
    port = port or 8899;
    
    nginx.acceptor = luaos.socket("tcp");
    if not nginx.acceptor then
        return false;
    end
    
    if ctx == nil then
        ctx = false;
    end
    return nginx.acceptor:listen(host, port, bind(on_socket_accept, ctx));
end

return nginx;

----------------------------------------------------------------------------

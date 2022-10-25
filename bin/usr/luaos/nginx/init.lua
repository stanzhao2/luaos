

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
	peer:send(table_concat(cache))
	peer:send(text)
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
	peer:send(table_concat(cache))
    if has_body then
        peer:send(data)
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
    
    local url    = conv.url.unescape(request:url())
    local uri    = string_split(url, '?')   
    local params = parse_url_params(uri[2])
    local path   = string_split(uri[1], '/')
    local depth  = #path
    
    if depth == 0 then
        depth = 1
        table_insert(path, "index")
    else
        local cnt = 0
        for i = 1, depth do
            if path[i] ~= ".." then
                cnt = cnt + 1
            else
                cnt = cnt - 1
            end
            if cnt < 0 then
                throw("Path depth attack")
            end
        end
    end
	
    local filename = ""
    for i = 1, depth do
        filename = filename .. "." .. path[i]
    end
    
    local others = path[depth]
    others = string_split(others, '.')
    
    ---读取静态文件
    if #others > 1 then
		local ext = others[#others]
        return on_http_download(peer, headers, filename, ext)
    end
	
    ---加载脚本文件
    local ok, script = pcall(require, _WWWROOT .. filename)
    if not ok then
		error(script)
        return on_http_error(peer, headers, _STATE_ERROR)
    end
    
    ---运行脚本文件
    local ok, result = pcall(script, request, headers, params)
    if not ok then
		error(script)
        return on_http_error(peer, headers, _STATE_ERROR)
    end
    
    return on_http_success(peer, headers)
end

----------------------------------------------------------------------------

local sessions = {}

local function on_http_callback(peer, request)
    local method = request:method()
    if method ~= "GET" and method ~= "POST" then
		peer:close();
        return false
    end
    
    local upgrade = request:is_upgrade()
    if upgrade then
        peer:close()
        return
    end
    
	local from = peer:endpoint()
    local ok, result = pcall(on_http_request, peer, request)
	if not ok then
		peer:close();
		return;
	end
	local url = request:url()
	if url then
		local method = request:method()
		print(method .. " " .. url .. " " .. result .. " from: " .. from)
	end
end

local function on_socket_error(peer, ec)
	if peer:is_open() then
		peer:close()
	end
	local fd = peer:id()
	if fd then
		sessions[fd] = nil
	end
end

local function on_socket_receive(peer, ec, data)
	if ec > 0 then
		on_socket_error(peer, ec);
		return;
	end
	
    local session = sessions[peer:id()]    
    if not session then
        return
    end
    
	local rsize = #data
    local request = session.parser
	while rsize > 0 do
		local errno, size = request:parse(data, bind(on_http_callback, peer))
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
	try {
		function()
			on_socket_receive(peer, ec, data)
		end
	}
	.catch {
		function(err)
			error(err)
			peer:close()
		end
	}
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
	peer:handshake(bind(on_ssl_handshake, peer));
end

----------------------------------------------------------------------------

local nginx = {}

function nginx.stop()
	if nginx.acceptor then
		nginx.acceptor:close();
		nginx.acceptor = nil
	end
	for k, v in pairs(sessions) do
		v.peer:close()
		v.peer = nil
		v.parser = nil
	end
	sessions = {}
end

function nginx.run(wwwroot, port, host, ctx)
	if nginx.acceptor then
		return false
	end
    if wwwroot then
        _WWWROOT = wwwroot
        _WWWROOT = string_gsub(_WWWROOT, '/', '.')
        _WWWROOT = string_gsub(_WWWROOT, '\\', '.')
    end
	host = host or "0.0.0.0"
	port = port or 8899
    
	nginx.acceptor = luaos.socket("tcp");
	if not nginx.acceptor then
		return false
	end
	if ctx == nil then
		ctx = false
	else
		trace("https is enabled, communication will be encrypted");
	end
	return nginx.acceptor:listen(host, port, bind(on_socket_accept, ctx));
end

return nginx

----------------------------------------------------------------------------

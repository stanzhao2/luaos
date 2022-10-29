

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

local luaos  = require("luaos")
local httpd  = require("http.parser")
local parser = luaos.class("http-parser")

----------------------------------------------------------------------------

function parser:__init(response)
    local handler = {}
    function handler.on_url(url)
        self._url = url
    end
    function handler.on_header(key, value)
        self._headers[key] = value
    end
    function handler.on_headers_complete()
        return
    end
    function handler.on_body(body)
        if body then
            table.insert(self._cache, body)
        end
    end
    function handler.on_state(code, text)
        self._state  = code
        self._reason = text
    end
    function handler.on_message_begin()
        self._state   = nil
        self._reason  = nil
        self._url     = nil
        self._body    = nil
        self._cache   = {}
        self._headers = {}
    end
    function handler.on_message_complete()
        if #self._cache > 0 then
            self._body = table.concat(self._cache)
        end
        if self._callback then
            self._callback(self)
        end
    end
    function handler.on_chunk_header(length)

    end
    function handler.on_chunk_complete()
        
    end
    if not response then
        self._parser = httpd.request(handler)
        return
    end
    self._parser = httpd.response(handler)
end

function parser:reset()
    return self._parser:reset()
end

function parser:method()
    return self._parser:method()
end

function parser:version()
    return self._parser:version()
end

function parser:headers()
    return self._headers
end

function parser:url()
    return self._url
end

function parser:is_keep_alive()
    return self._parser:should_keep_alive()
end

function parser:body()
    return self._body
end

function parser:is_upgrade()
    return self._parser:is_upgrade()
end

function parser:state_code()
    return self._parser:status_code()
end

function parser:error()
    return self._parser:error()
end

function parser:parse(data, callback)
    self._callback = callback
    local do_size = self._parser:execute(data)
    return self._parser:error(), do_size
end

return parser

----------------------------------------------------------------------------

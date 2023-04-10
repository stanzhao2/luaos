

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

local M = {}
local bit = {
    band = function(a, b)
        return a & b;
    end;
    bor = function(a, b)
        return a | b;
    end;
    rshift = function(a, b)
        return a >> b;
    end;
    lshift = function(a, b)
        return a << b;
    end;
}

function M.unicode_to_utf8(convertStr)
    if type(convertStr)~="string" then
        return convertStr
    end
    local resultStr=""
    local i=1
    while true do
        local num1=string.byte(convertStr,i)
        local unicode
        
        if num1~=nil and string.sub(convertStr,i,i+1)=="\\u" then
            unicode=tonumber("0x"..string.sub(convertStr,i+2,i+5))
            i=i+6
        elseif num1~=nil then
            unicode=num1
            i=i+1
        else
            break
        end
        -- print(unicode)
  
        if unicode <= 0x007f then
            resultStr=resultStr..string.char(bit.band(unicode,0x7f))
        elseif unicode >= 0x0080 and unicode <= 0x07ff then
            resultStr=resultStr..string.char(bit.bor(0xc0,bit.band(bit.rshift(unicode,6),0x1f)))
            resultStr=resultStr..string.char(bit.bor(0x80,bit.band(unicode,0x3f)))
        elseif unicode >= 0x0800 and unicode <= 0xffff then
            resultStr=resultStr..string.char(bit.bor(0xe0,bit.band(bit.rshift(unicode,12),0x0f)))
            resultStr=resultStr..string.char(bit.bor(0x80,bit.band(bit.rshift(unicode,6),0x3f)))
            resultStr=resultStr..string.char(bit.bor(0x80,bit.band(unicode,0x3f)))
        end
    end
    resultStr=resultStr..'\0'
    return resultStr
end

function M.utf8_to_unicode(convertStr)
    if type(convertStr)~="string" then
        return convertStr
    end
    local resultStr=""
    local i=1
    local num1=string.byte(convertStr,i)
    
    while num1~=nil do
        local tempVar1,tempVar2
        if num1 >= 0x00 and num1 <= 0x7f then
            tempVar1=num1
            tempVar2=0
        elseif bit.band(num1,0xe0)== 0xc0 then
            local t1 = 0
            local t2 = 0
            t1 = bit.band(num1,bit.rshift(0xff,3))
            i=i+1
            num1=string.byte(convertStr,i)
            t2 = bit.band(num1,bit.rshift(0xff,2))
            tempVar1=bit.bor(t2,bit.lshift(bit.band(t1,bit.rshift(0xff,6)),6))
            tempVar2=bit.rshift(t1,2)
        elseif bit.band(num1,0xf0)== 0xe0 then
            local t1 = 0
            local t2 = 0
            local t3 = 0
            t1 = bit.band(num1,bit.rshift(0xff,3))
            i=i+1
            num1=string.byte(convertStr,i)
            t2 = bit.band(num1,bit.rshift(0xff,2))
            i=i+1
            num1=string.byte(convertStr,i)
            t3 = bit.band(num1,bit.rshift(0xff,2))
            tempVar1=bit.bor(bit.lshift(bit.band(t2,bit.rshift(0xff,6)),6),t3)
            tempVar2=bit.bor(bit.lshift(t1,4),bit.rshift(t2,2))
        end
        resultStr=resultStr..string.format("\\u%02x%02x",tempVar2,tempVar1)
        i=i+1
        num1=string.byte(convertStr,i)
    end
    return resultStr
end

return M;

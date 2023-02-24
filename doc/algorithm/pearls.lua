

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

----------------------------------------------------------------------------

local game   = {}
local insert = table.insert;
local random = math.random;

----------------------------------------------------------------------------

function game.take(pearls)
	local whichrow  = 0;
	local howmany   = 0;
	local ones      = 0;
	local multiples = 0;
	
    --统计只有一个珍珠的行数及有多个珍珠的行数
	for i = 1, 7 do
        if pearls[i] == 1 then
            ones = ones + 1;
        elseif pearls[i] > 1 then
            multiples = multiples + 1;
            whichrow = i;
        end
	end	
	assert(#pearls == 7 and (ones > 0 or multiples > 0));
	
	--如果只有一行有多个珍珠
    if multiples == 1 then
        ones = ones % 2;
        howmany = pearls[whichrow] - 1 + ones;
        return whichrow, howmany;
    end
	
    local xorarray = {0, 0, 0, 0, 0, 0, 0};
    xorarray[1] = pearls[1] - (((((pearls[2] ~ pearls[3]) ~ pearls[4]) ~ pearls[5]) ~ pearls[6]) ~ pearls[7]);
    xorarray[2] = pearls[2] - (((((pearls[1] ~ pearls[3]) ~ pearls[4]) ~ pearls[5]) ~ pearls[6]) ~ pearls[7]);
    xorarray[3] = pearls[3] - (((((pearls[2] ~ pearls[1]) ~ pearls[4]) ~ pearls[5]) ~ pearls[6]) ~ pearls[7]);
    xorarray[4] = pearls[4] - (((((pearls[2] ~ pearls[3]) ~ pearls[1]) ~ pearls[5]) ~ pearls[6]) ~ pearls[7]);
    xorarray[5] = pearls[5] - (((((pearls[2] ~ pearls[3]) ~ pearls[4]) ~ pearls[1]) ~ pearls[6]) ~ pearls[7]);
    xorarray[6] = pearls[6] - (((((pearls[2] ~ pearls[3]) ~ pearls[4]) ~ pearls[5]) ~ pearls[1]) ~ pearls[7]);
    xorarray[7] = pearls[7] - (((((pearls[2] ~ pearls[3]) ~ pearls[4]) ~ pearls[5]) ~ pearls[6]) ~ pearls[1]);
	
	--选择最优的珠子
    local rowarray = {};
    local verifys = (((((pearls[1] ~ pearls[2]) ~ pearls[3]) ~ pearls[4]) ~ pearls[5]) ~ pearls[6]) ~ pearls[7];	
    if verifys == 0 then
        for i = 1, 7 do
            if pearls[i] > 0 then
				insert(rowarray, i);
			end
		end
		--没有正确拿法，随机拿
        whichrow = rowarray[random(1, #rowarray)];
        howmany  = random(1, pearls[whichrow]);
	 else
        local numberarray = {};
        for i = 1, 7 do
            if xorarray[i] > 0 then
				insert(rowarray, i);
				insert(numberarray, xorarray[i]);
			end
        end
		local whichone = random(1, #rowarray);
        whichrow = rowarray[whichone];
        howmany  = numberarray[whichone];
	end
	return whichrow, howmany;
end

----------------------------------------------------------------------------

return game

----------------------------------------------------------------------------

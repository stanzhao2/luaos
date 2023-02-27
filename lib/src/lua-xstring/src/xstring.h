

/************************************************************************************
**
** Copyright 2021-2023 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
************************************************************************************/

#pragma once

#include "wrapper.h"
#include "string/bstrwrap.h"

class lua_string : public lua_userdata {
	CBString _str;
	typedef lua_userdata parent;
	const char* check_string(int index) const;

public:
	static const luaL_Reg* methods();
	inline static const char* name() {
		return "bstring";
	}
	int split() const;
	int concat() const;
	int tostring() const;
	int eq() const;
	int lt() const;
	int le() const;
	int len() const;
	int caselesseq() const;
	int find() const;
	int caselessfind() const;
	int reversefind() const;
	int caselessreversefind() const;
	int findreplace();
	int findreplacecaseless();
	int trim();
	int ltrim();
	int rtrim();
	int toupper();
	int tolower();

public:
	CBString& lowest_layer() {
		return _str;
	}
	lua_string(lua_State* L);
	lua_string(lua_State* L, const char *s);
	lua_string(lua_State* L, const char *s, size_t len);
	lua_string(lua_State* L, const lua_string& b);
};

/***********************************************************************************/

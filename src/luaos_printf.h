
/********************************************************************************
** 
** Copyright 2021-2022 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
********************************************************************************/

#pragma once

enum struct color_type {
  normal  = 0,
  yellow  = 1,
  red     = 2
};

LUALIB_API int lua_os_print(lua_State* L);

LUALIB_API int lua_os_error(lua_State* L);

LUALIB_API int lua_os_trace(lua_State* L);

void my_printf(color_type type_color, bool prefix, const char* fmt, ...);

#define _printf my_printf

/*******************************************************************************/

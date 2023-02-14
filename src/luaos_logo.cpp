
/************************************************************************************
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
************************************************************************************/

#include "luaos.h"

/***********************************************************************************/

#ifdef _MSC_VER
#include "luaos_console.h"
#endif

/***********************************************************************************/

static void logo_print(const std::string& str, color_type color)
{
#ifdef _MSC_VER
  console::instance()->print(str, color);
#else
  printf("\033[1;36m%s\033[0m", str.c_str());  //缺省色(青色)
#endif
}

/***********************************************************************************/

void display_logo()
{
  std::string logo = R"(
  '##:::::::'##::::'##::::'###:::::'#######:::'######::  (R)2.0
   ##::::::: ##:::: ##:::'## ##:::'##.... ##:'##... ##:
   ##::::::: ##:::: ##::'##:. ##:: ##:::: ##: ##:::..::
   ##::::::: ##:::: ##:'##:::. ##: ##:::: ##:. ######::
   ##::::::: ##:::: ##: #########: ##:::: ##::..... ##:  __   ____________
   ##::::::: ##:::: ##: ##.... ##: ##:::: ##:'##::: ##:  | \\  |______  | 
   ########:. #######:: ##:::: ##:. #######::. ######:: .|  \\_|______  | 
  ........:::.......:::..:::::..:::.......::::......:::....................
  )";

  time_t now = time(0);
  struct tm* ptm = localtime(&now);
  char copyright[128];
  sprintf(copyright, "Copyright (C) 2021-%d, All right reserved\n\n", ptm->tm_year + 1900);
  logo += copyright;

  char version[128];
  sprintf(version, "  > + Dependencies: %s\n\n", LUA_RELEASE);
  logo += version;
  logo_print(logo.c_str(), color_type::normal);
}

/***********************************************************************************/

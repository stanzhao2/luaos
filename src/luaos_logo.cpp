
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

static void logo_print(const char* str, color_type color)
{
#ifdef _MSC_VER
  const std::string info(str);
  console::instance()->print(info, color);
#else
  printf("\033[1;36m%s\033[0m", str);  //缺省色(青色)
#endif
}

/***********************************************************************************/

void display_logo()
{
  char version[64];
  snprintf(version, sizeof(version), "LuaOS %d.%d (R)", LOS_VERSION_MAJOR, LOS_VERSION_MINOR);

  time_t now = time(0);
  struct tm* ptm = localtime(&now);
  char copyright[128];
  snprintf(copyright, sizeof(copyright), "Copyright (C) 2021-%d, All right reserved", ptm->tm_year + 1900);

  char tlsver[128];
  snprintf(tlsver, sizeof(tlsver), "TLS version: 1.2 (openssl %s)", SHLIB_VERSION_NUMBER);

  char depends[128];
  snprintf(depends, sizeof(depends), "Dependencies: %s", LUA_RELEASE);

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "\n> %s\n> %s\n> %s\n> %s\n\n", version, copyright, tlsver, depends);
  logo_print(buffer, color_type::normal);
}

/***********************************************************************************/

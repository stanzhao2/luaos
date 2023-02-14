
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

#pragma once

#include <windows.h>
#include <conv.h>
#include <string>
#include "luaos_color.h"

/***********************************************************************************/

class console final
{
  inline console()
  {
    SetConsoleOutputCP(65001);
    CONSOLE_SCREEN_BUFFER_INFO si;
    _handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(_handle, &si);
    _color = si.wAttributes;
  }
  WORD   _color;
  HANDLE _handle;

public:
  virtual ~console() {
    SetConsoleTextAttribute(_handle, _color);
  }
  inline static console* instance() {
    static console _console;
    return &_console;
  }
  void print(const std::string& data, color_type color)
  {
    WORD print_color = FOREGROUND_INTENSITY;
    switch (color) {
    case color_type::red:
      print_color |= FOREGROUND_RED;
      break;
    case color_type::yellow:
      print_color |= FOREGROUND_RED;
      print_color |= FOREGROUND_GREEN;
      break;
    default:
      print_color |= FOREGROUND_GREEN;
      print_color |= FOREGROUND_BLUE;
      break;
    }
    SetConsoleTextAttribute(_handle, print_color);
    printf(data.c_str());
    SetConsoleTextAttribute(_handle, _color);
  }
};

/***********************************************************************************/

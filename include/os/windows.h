

/************************************************************************************
**
** Copyright 2021 stanzhao
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

/***********************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <chrono>
#include <io.h>
#include <direct.h>
#include <string>

typedef unsigned __int64 __uint64;

/***********************************************************************************/

#define chdir _chdir

namespace os
{
  inline static void* dlopen(const char* filename)
  {
    return (void*)LoadLibraryA(filename);
  }
  inline static void* dlsym(void* mod, const char* funcname)
  {
    return GetProcAddress((HMODULE)mod, funcname);
  }
  inline static void dlclose(void* mod)
  {
    FreeLibrary((HMODULE)mod);
  }
  template <class T, class... Args>
  inline static T invoke(void* fn, Args... args)
  {
    typedef T(*_func)(Args...);
    return (T)(fn ? ((_func)fn)(args...) : T(0));
  }
  template <class T, class... Args>
  inline static T invoke(void* mod, const char* name, Args... args)
  {
    return invoke<T>(dlsym(mod, name), args...);
  }
  inline static size_t nanoseconds()
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()
    ).count();
  }
  inline static size_t microseconds()
  {
    return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()
    ).count();
  }
  inline static size_t milliseconds()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()
    ).count();
  }
  inline static size_t seconds()
  {
    return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()
    ).count();
  }
}

namespace dir
{
  inline bool make(const char* path)
  {
    HANDLE hfind;
    WIN32_FIND_DATAA data;
    hfind = FindFirstFileA(path, &data);

    if (hfind != INVALID_HANDLE_VALUE) {
      FindClose(hfind);
      return true;
    }
    return (_mkdir(path) == 0);
  }

  inline const char* current(char* path, int count)
  {
    char filename[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, filename, MAX_PATH);
    char* pos = strrchr(filename, '\\');
    if (!pos) {
      return 0;
    }
    *pos = 0;
    if ((int)strlen(filename) + 1 > count)
      return 0;
    return (strcpy_s(path, MAX_PATH, filename), path);
  }

  inline const std::string& current()
  {
    static thread_local std::string cur_path;
    if (!cur_path.empty())
      return cur_path;

    char filepath[MAX_PATH] = { 0 };
    cur_path = current(filepath, MAX_PATH) ? filepath : ".\\";
    return cur_path;
  }
}

/***********************************************************************************/

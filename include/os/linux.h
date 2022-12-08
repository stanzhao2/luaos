

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

#if defined(os_apple)
# include <mm_malloc.h>   //malloc/free
# include <mach/clock.h>  //host_get_clock_service
# include <mach/mach.h>   //host_get_clock_service
#else
# include <sys/types.h>
# include <malloc.h>
# include <memory>
# include <sys/time.h>
#endif

#include <sys/stat.h>
#include <string>
#include <unistd.h>
#include <dlfcn.h>        //dlopen, dlclose, dlsym
#include <time.h>
#include <chrono>
#include <dirent.h>
#include <linux/kernel.h>

/***********************************************************************************/

typedef long long __int64;
typedef unsigned long long __uint64;

/***********************************************************************************/

namespace os
{
  inline static void* dlopen(const char* filename)
  {
    return (void*)::dlopen(filename, RTLD_LAZY);
  }
  inline static void* dlsym(void* mod, const char* funcname)
  {
    return ::dlsym(mod, funcname);
  }
  inline static void dlclose(void* mod)
  {
    ::dlclose(mod);
  }
  template <class T, class... Args>
  static T invoke(void* fn, Args... args)
  {
    typedef T(*_func)(Args...);
    return (T)(fn ? ((_func)fn)(args...) : T(0));
  }
  template <class T, class... Args>
  static  T invoke(void* mod, const char* name, Args... args)
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
    DIR* dir = opendir(path);
    if (dir != 0) {
      closedir(dir);
      return true;
    }
    mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    return (mkdir(path, mode) == 0);
  }

  inline const char* current(char* path, int count)
  {
    int i;
    int rslt = readlink("/proc/self/exe", path, count - 1);
    if (rslt < 0 || (rslt >= count - 1))
      return 0;

    path[rslt] = '\0';
    for (i = rslt; i >= 0; i--) {
      if (path[i] == '/') {
        path[i + 1] = '\0';
        break;
      }
    }
    return path;
  }

  inline const std::string& current()
  {
    static thread_local std::string cur_path;
    if (!cur_path.empty())
      return cur_path;

    char filepath[1024] = { 0 };
    cur_path = current(filepath, 1024) ? filepath : "./";
    return cur_path;
  }
}

/***********************************************************************************/

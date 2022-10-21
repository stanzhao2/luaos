

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

#include <cstdint>
#include <assert.h>

/***********************************************************************************/

#if defined(__APPLE__) && defined(__GNUC__)
# define os_macx
#elif defined(__MACOSX__)
# define os_macx
#elif defined(macintosh)
# define os_mac9
#elif defined(__CYGWIN__)
# define os_cygwin
#elif defined(WIN64) || defined(_WIN64) || defined(__WIN64__) 
# define os_win64
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) 
# define os_win32
#elif defined(__sun) || defined(sun)
# define os_solaris
#elif defined(hpux)  || defined(__hpux)
# define os_hpux
#elif defined(__linux__) || defined(__linux)
# define os_linux
#elif defined(__FreeBSD__)
# define os_freebsd
#elif defined(__NetBSD__)
# define os_netbsd
#elif defined(__OpenBSD__)
# define os_openbsd
#else
# error "Has not been ported to this OS."
#endif

/***********************************************************************************/

#if defined(os_mac9) || defined(os_macx)
# define os_apple
#endif

#if defined(os_freebsd) || defined(os_netbsd) || defined(os_openbsd)
# define os_bsdx
#endif

#if defined(os_win32) || defined(os_win64)
# define os_windows
#endif

#if defined(os_windows)
# include "windows.h"  //include for windows
#else
# include "linux.h"    //include for not windows
#endif

/***********************************************************************************/

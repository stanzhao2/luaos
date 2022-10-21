
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

#include <string>
#include <stdlib.h>
#include <cstring>

#define _ini_size 1024
#define _min_size(a, b)   ((a) < (b) ? (a) : (b))
#define _max_size(a, b)   ((a) < (b) ? (b) : (a))
#define _mod_size(n, s)   (n & (s - 1))
#define _is_power_of_2(n) (n && ((n & (n - 1)) == 0))

/*******************************************************************************/

class circular_buffer
{
  static size_t roundup_power_of_2(size_t len)
  {
    if (_is_power_of_2(len)) {
      return len;
    }
    size_t maxv = ~0;
    size_t andv = ~(maxv & (maxv >> 1));
    while ((andv & len) == 0) andv = andv >> 1;
    return (andv << 1);
  }
  void normalize()
  {
    size_t nu = size();
    if (nu == 0) {
      return;
    }
    size_t nr = _mod_size(_nr, _nsize);
    size_t nw = _mod_size(_nw, _nsize);
    if (nr < nw) {
      _nr = nr; _nw = nw;
      return;
    }
    char* buffer = (char*)malloc(nw);
    if (!buffer) {
      throw("no memory");
    }
    _nr = 0; _nw = nu;
    memcpy (buffer, _data, nw);
    memmove(_data,  _data + nr, _nsize - nr);
    memcpy (_data + _nsize - nr, buffer, nw);
    free(buffer);
  }
  size_t resize(size_t len)
  {
    assert(size() < len);
    assert(_is_power_of_2(len));
    if (len == _nsize) {
      return len;
    }
    normalize();
    char* data = (char*)realloc(_data, len);
    if (!data) {
      throw("no memory");
    }
    _nsize = len;
    return (_data = data), len;
  }
  char*  _data;
  size_t _nr, _nw;
  size_t _nsize, _nmax, _resev;
  circular_buffer(const circular_buffer&) = delete;

public:
  explicit circular_buffer(size_t nmax = 0)
    : _nr(0), _nw(0)
  {
    _nmax  = nmax;
    _nsize = _ini_size;
    _resev = _nsize << 3;
    if (_nmax)
    {
      _nmax  = roundup_power_of_2(_nmax);
      _nsize = _min_size(_nsize, _nmax);
      _resev = _min_size(_nsize << 3, _nmax);
    }
    _data = (char*)malloc(_nsize);
  }
  virtual ~circular_buffer() {
    free(_data);
  }
  inline size_t empty() const {
    return size() == 0;
  }
  inline size_t size()  const {
    return _nw - _nr;
  }
  inline size_t erase(size_t len) {
    return read(0, len);
  }
  inline const char* data() {
    return normalize(), (_data + _nr);
  }
  inline void clear()
  {
    _nr = _nw = 0;
    resize(_min_size(_nsize, _resev));
    assert(_nw == 0);
    assert(_nr == 0);
  }
  size_t reserve(size_t len)
  {
    _resev = roundup_power_of_2(len);
    if (_nmax) {
      _resev = _min_size(_resev, _nmax);
    }
    return _resev;
  }
  size_t read(char* buffer, size_t len)
  {
    len = _min_size(len, size());
    size_t m = _mod_size(_nr, _nsize);
    size_t n = _min_size(len, _nsize - m);

    if (buffer) {
      memcpy(buffer, _data + m, n);
      memcpy(buffer + n, _data, len - n);
    }
    if ((_nr += len) == _nw) {
      clear();
    }
    return len;
  }
  size_t read(std::string& data, size_t len)
  {
    len = _min_size(len, size());
    data.resize(len);
    return read((char*)data.c_str(), len);
  }
  size_t write(const char* buffer, size_t len)
  {
    size_t total = len;
    len = _min_size(len, _nsize - size());
    if (len < total)
    {
      size_t n = roundup_power_of_2(size() + total);
      if (!_nmax || n <= _nmax)
      {
        if (resize(n) == n) {
          len = total;
        }
      }
    }
    size_t m = _mod_size(_nw, _nsize);
    size_t n = _min_size(len, _nsize - m);

    memcpy(_data + m, buffer, n);
    memcpy(_data, (char*)buffer + n, len - n);
    _nw += len;
    return len;
  }
  size_t write(const std::string& data)
  {
    return write(data.c_str(), data.size());
  }
};

/*******************************************************************************/

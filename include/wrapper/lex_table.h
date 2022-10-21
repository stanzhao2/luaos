

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

#include <string>
#include <map>
#include "lex_any.h"

typedef struct __tablekey {
#ifdef __ANDROID__
  inline __tablekey& operator=(const __tablekey& r) {
    *(long long*)&index = r.index;
    *(std::string*)&name = r.name;
    *(bool*)&isnumber = r.isnumber;
    return *this;
  }
#endif
  inline __tablekey(const std::string& key)
    : index(0)
    , name(key)
    , isnumber(false) {
  }
  inline __tablekey(long long key)
    : index(key)
    , isnumber(true) {
  }
  inline __tablekey(const __tablekey& r)
    : index(r.index)
    , name(r.name)
    , isnumber(r.isnumber) {
  }
  inline bool is_number_key() const {
    return isnumber;
  }
  inline bool operator<(const __tablekey& r) const {
    return (index != r.index) ? (index < r.index) : (name < r.name);
  }
  const bool isnumber;
  const long long index;
  const std::string name;
} table_key;

template <typename _Key = table_key> class lua_table final {
  std::map<_Key, std::any> _imap;

public:
  template <typename _Ty>
  inline static _Ty cast(const std::any& v) {
    try { return std::any_cast<_Ty>(v); }
    catch (...) {}
    return _Ty();
  }
  template <typename _Ty>
  inline static bool is(const std::any& v) noexcept {
#if __cpp_rtti
    return typeid(_Ty) == v.type();
#else
    try { return std::any_cast<_Ty>(v), true; }
    catch (...) {}
    return false;
#endif
  }
  typedef typename std::map<_Key, std::any>::iterator iterator;

  typedef typename std::map<_Key, std::any>::const_iterator const_iterator;

  inline iterator begin() noexcept {
    return _imap.begin();
  }
  inline const_iterator begin() const noexcept {
    return _imap.begin();
  }
  inline iterator end() noexcept {
    return _imap.end();
  }
  inline const_iterator end() const noexcept {
    return _imap.end();
  }
  inline iterator find(size_t i) noexcept {
    return _imap.find(i);
  }
  inline iterator find(const std::string& name) noexcept {
    return _imap.find(name);
  }
  inline iterator erase(iterator& iter) noexcept {
    return _imap.erase(iter);
  }
  inline const_iterator erase(const_iterator& iter) noexcept {
    return _imap.erase(iter);
  }
  inline lua_table(const lua_table& r) noexcept
    : _imap(r._imap) {
  }
  inline lua_table& operator=(const lua_table& r) noexcept {
    _imap = r._imap;
    return *this;
  }
  inline lua_table() = default;

public:
  inline std::any& operator[](size_t i) noexcept {
    return _imap[i];
  }
  inline std::any& operator[](const std::string& name) noexcept {
    return _imap[name];
  }
  inline void clear() noexcept {
    _imap.clear();
  }
  inline bool empty() const noexcept {
    return _imap.empty();
  }
  inline iterator erase(const_iterator iter) noexcept {
    return _imap.erase(iter);
  }
  inline iterator erase(const_iterator begin, const_iterator end) noexcept {
    return _imap.erase(begin, end);
  }
  inline size_t erase(size_t i) noexcept {
    return _imap.erase(i);
  }
  inline size_t erase(const std::string& name) noexcept {
    return _imap.erase(name);
  }
  inline size_t size() const noexcept {
    return _imap.size();
  }
  inline const std::any get(size_t i) const noexcept {
    static const std::any _any;
    auto iter = _imap.find(i);
    return (iter == _imap.end() ? _any : iter->second);
  }
  inline const std::any get(const std::string& name) const noexcept {
    static const std::any _any;
    auto iter = _imap.find(name);
    return (iter == _imap.end() ? _any : iter->second);
  }
  inline bool exist(size_t i) const noexcept {
    return (_imap.find(i) != _imap.end());
  }
  inline bool exist(const std::string& name) const noexcept {
    return (_imap.find(name) != _imap.end());
  }
  template <typename _Ty> const _Ty get(size_t i) const {
    return cast<_Ty>(get(i));
  }
  template <typename _Ty> const _Ty get(const std::string& name) const {
    return cast<_Ty>(get(name));
  }
};


#pragma once

#include <locale>
#include <string>
#include <memory>
#include <codecvt>

inline static bool is_utf8(const char* p, size_t n = 0)
{
  unsigned char chr;
  size_t i = 0, follow = 0;
  for (; n ? (i < n) : (p[i] != 0); i++) {
    chr = p[i];
    if (follow == 0) {
      if (chr > 0x80) {
        if (chr == 0xfc || chr == 0xfd) {
          follow = 5;
        }
        else if (chr >= 0xf8) {
          follow = 4;
        }
        else if (chr >= 0xf0) {
          follow = 3;
        }
        else if (chr >= 0xe0) {
          follow = 2;
        }
        else if (chr >= 0xc0) {
          follow = 1;
        }
        else {
          return false;
        }
      }
    } else {
      follow--;
      if ((chr & 0xC0) != 0x80) {
        return false;
      }
    }
  }
  return (follow == 0);
}

inline static std::string wcs_to_mbs(const std::wstring& wstr, const char* locale = 0)
{
  std::string ret;
  std::mbstate_t state = {};
  const wchar_t* src = wstr.c_str();
  size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
  if (static_cast<size_t>(-1) != len)
  {
    std::unique_ptr<char[]> buff(new char[len + 1]);
    len = std::wcsrtombs(buff.get(), &src, len, &state);
    if (static_cast<size_t>(-1) != len) {
      ret.assign(buff.get(), len);
    }
  }
  return ret;
}

inline static std::wstring mbs_to_wcs(const std::string& str, const char* locale = 0)
{
  std::wstring ret;
  std::mbstate_t state = {};
  const char* src = str.c_str();
  size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
  if (static_cast<size_t>(-1) != len)
  {
    std::unique_ptr<wchar_t[]> buff(new wchar_t[len + 1]);
    len = std::mbsrtowcs(buff.get(), &src, len, &state);
    if (static_cast<size_t>(-1) != len) {
      ret.assign(buff.get(), len);
    }
  }
  return ret;
}

inline static std::wstring utf8_to_wcs(const std::string& str)
{
  std::wstring ret;
  try {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> wcv;
    ret = wcv.from_bytes(str);
  } catch (...){}
  return ret;
}

inline static std::string wcs_to_utf8(const std::wstring& wstr)
{
  std::string ret;
  try {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> wcv;
    ret = wcv.to_bytes(wstr);
  } catch (...){}
  return ret;
}

inline static std::string mbs_to_utf8(const std::string& str)
{
  std::wstring wstr = mbs_to_wcs(str);
  return wcs_to_utf8(wstr);
}

inline static std::string utf8_to_mbs(const std::string& str)
{
  std::wstring wstr = utf8_to_wcs(str);
  return wcs_to_mbs(wstr);
}

//全角转半角
inline static std::string sbc_to_dbc(const char* s)
{
  std::string output;
  for (; *s; ++s) {
    if ((*s & 0xff) == 0xa1 && (*(s + 1) & 0xff) == 0xa1) { //全角空格
      output += 0x20;
      ++s;
    }
    else if ((*s & 0xff) == 0xa3 && (*(s + 1) & 0xff) >= 0xa1 && (*(s + 1) & 0xff) <= 0xfe) { //ascii码中其它可显示字符
      output += (*++s - 0x80);
    }
    else {
      if (*s < 0) //如果是中文字符，则拷贝两个字节
        output += (*s++);
      output += (*s);
    }
  }
  return std::move(output);
}

//全角转半角
inline static std::string sbc_to_dbc(const std::string& str)
{ 
  return sbc_to_dbc(str.c_str());
}

//半角转全角
inline static std::string dbc_to_sbc(const char* s)
{
  std::string output;
  for (; *s; ++s) {
    if ((*s & 0xff) == 0x20) { //半角空格
      output += (char)0xa1;
      output += (char)0xa1;
    }
    else if ((*s & 0xff) >= 0x21 && (*s & 0xff) <= 0x7e) {
      output += (char)0xa3;
      output += (*s + (char)0x80);
    }
    else {
      if (*s < 0)    //如果是中文字符，则拷贝两个字节
        output += (*s++);
      output += (*s);
    }
  }
  return std::move(output);
}

//半角转全角
inline static std::string dbc_to_sbc(const std::string& str)
{ 
  return dbc_to_sbc(str.c_str());
}

inline static std::string utf8_to_dbc(const char* s, char placeholder = 0)
{
  std::string output;
  for (size_t j = 0; s[j] != 0; j++)
  {
    if (((s[j] & 0xf0) ^ 0xe0) != 0) {
      output += s[j];
      continue;
    }
    int c = (s[j] & 0xf) << 12 | ((s[j + 1] & 0x3f) << 6 | (s[j + 2] & 0x3f));
    if (c == 0x3000) { // blank
      if (placeholder) output += placeholder;
      output += ' ';
    }
    else if (c >= 0xff01 && c <= 0xff5e) { // full char
      if (placeholder) output += placeholder;
      char new_char = c - 0xfee0;
      output += new_char;
    }
    else { // other 3 bytes char
      output += s[j];
      output += s[j + 1];
      output += s[j + 2];
    }
    j = j + 2;
  }
  return std::move(output);
}

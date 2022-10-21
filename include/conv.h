
#pragma once

#include <locale.h>
#include <string>

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
    }
    else {
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
  if (wstr.empty()) {
    return std::string();
  }
  if (!locale) {
    locale = "";
  }
  locale = setlocale(LC_CTYPE, locale);

  std::string str;
  size_t bytes = wcstombs(0, wstr.c_str(), 0) + 1;
  char* pmbuff = new char[bytes + 1];

  size_t size = wcstombs(pmbuff, wstr.c_str(), bytes);
  if (size != (size_t)(-1)) {
    str.append(pmbuff, size);
  }
  delete[] pmbuff;
  return std::move(str);
}

inline static std::wstring mbs_to_wcs(const std::string& str, const char* locale = 0)
{
  if (str.empty()) {
    return std::wstring();
  }
  if (!locale) {
    locale = "";
  }
  locale = setlocale(LC_CTYPE, locale);

  std::wstring wstr;
  size_t bytes = mbstowcs(0, str.c_str(), 0) + 1;
  wchar_t* pmbuff = new wchar_t[bytes + 1];

  size_t size = mbstowcs(pmbuff, str.c_str(), bytes);
  if (size != (size_t)(-1)) {
    wstr.append(pmbuff, size);
  }
  delete[] pmbuff;
  return std::move(wstr);
}

inline static std::wstring utf8_to_wcs(const std::string& str)
{
  std::wstring wstr;
  unsigned char chr;
  wchar_t wchr = 0;
  wchar_t* pwb = new wchar_t[str.size() + 1];
  wchar_t* pwc = pwb;
  const char* p = str.c_str();
  if (!pwb) {
    return wstr;
  }
  size_t follow = 0;
  for (size_t i = 0; p[i] != 0; i++) {
    chr = p[i];
    if (follow == 0) {
      wchr = 0;
      if (chr > 0x80) {
        if (chr == 0xfc || chr == 0xfd) {
          follow = 5;
          wchr = chr & 0x01;
        }
        else if (chr >= 0xf8) {
          follow = 4;
          wchr = chr & 0x03;
        }
        else if (chr >= 0xf0) {
          follow = 3;
          wchr = chr & 0x07;
        }
        else if (chr >= 0xe0) {
          follow = 2;
          wchr = chr & 0x0f;
        }
        else if (chr >= 0xc0) {
          follow = 1;
          wchr = chr & 0x1f;
        }
        else {
          follow = -1;
          break;
        }
      }
      else {
        *pwc++ = chr;
      }
    }
    else {
      if ((chr & 0xC0) != 0x80) {
        return L"";
      }
      wchr = (wchr << 6) | (chr & 0x3f);
      if (--follow == 0) {
        *pwc++ = wchr;
      }
    }
  }
  if (follow == 0) {
    wstr.append(pwb, pwc - pwb);
  }
  delete[] pwb;
  return std::move(wstr);
}

inline static std::string wcs_to_utf8(const std::wstring& wstr)
{
  typedef unsigned char uchr;
  typedef unsigned int  wchr;
  if (wstr.empty()) {
    return std::string();
  }
  std::string str;
  size_t wlen = wstr.size();
  uchr* obuff = new uchr[wlen * 3 + 1];
  if (!obuff) {
    return std::string();
  }
  uchr* t = obuff;
  wchar_t* us = (wchar_t*)wstr.c_str();
  size_t lwchr = sizeof(wchar_t);
  wchr unmax = (lwchr >= 4 ? 0x110000 : 0x10000);
  for (size_t i = 0; i < wlen; i++) {
    wchr w = us[i];
    if (w <= 0x7f) {
      *t++ = (uchr)w;
      continue;
    }
    if (w <= 0x7ff) {
      *t++ = 0xc0 | (uchr)(w >> 6);
      *t++ = 0x80 | (uchr)(w & 0x3f);
      continue;
    }
    if (w <= 0xffff) {
      *t++ = 0xe0 | (uchr)(w >> 12);
      *t++ = 0x80 | (uchr)((w >> 6) & 0x3f);
      *t++ = 0x80 | (uchr)(w & 0x3f);
      continue;
    }
    if (w < unmax) {
      *t++ = 0xf0 | (uchr)((w >> 18) & 0x07);
      *t++ = 0x80 | (uchr)((w >> 12) & 0x3f);
      *t++ = 0x80 | (uchr)((w >> 6) & 0x3f);
      *t++ = 0x80 | (uchr)(w & 0x3f);
      continue;
    }
  }
  str.append((char*)obuff, t - obuff);
  delete[]obuff;
  return std::move(str);
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


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

#include "rc4.h"
#include "quicklz.h"
#include "circular_buffer.h"

/*******************************************************************************

+---------------------------------------------------------------+
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|H| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |              (16)             |
|N|V|V|V|       |S|             |    (if Payload len == 127)    |
| |1|2|3|       |H|             |                               |
+-+-+-+-+-------+-+-------------+-------------------------------+
|                Hash-value, if HASH set to 1                   |
+-------------------------------+-------------------------------+
|                         Payload Data                          |
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
:                   Payload Data continued ...                  :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                   Payload Data continued ...                  |
+---------------------------------------------------------------+

********************************************************************************/

#ifndef IWORDS_BIG_ENDIAN
#ifdef _BIG_ENDIAN_
#if _BIG_ENDIAN_
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
#if defined(__hppa__) || \
  defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
  (defined(__MIPS__) && defined(__MIPSEB__)) || \
  defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
  defined(__sparc__) || defined(__powerpc__) || \
  defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
#define IWORDS_BIG_ENDIAN  0
#endif
#endif

#ifndef IWORDS_MUST_ALIGN
#if defined(__i386__) || defined(__i386) || defined(_i386_)
#define IWORDS_MUST_ALIGN 0
#elif defined(_M_IX86) || defined(_X86_) || defined(__x86_64__)
#define IWORDS_MUST_ALIGN 0
#elif defined(__amd64) || defined(__amd64__)
#define IWORDS_MUST_ALIGN 0
#else
#define IWORDS_MUST_ALIGN 1
#endif
#endif

/*******************************************************************************/

namespace eth
{
  typedef unsigned char   u8;
  typedef unsigned short  u16;
  typedef unsigned int    u32;

  //---------------------------------------------------------------------
  // BYTE ORDER & ALIGNMENT
  //---------------------------------------------------------------------

  /* encode 8 bits unsigned int */
  static __inline char *encode8u(char *p, u8 c)
  {
    *(u8*)p++ = c;
    return p;
  }

  /* decode 8 bits unsigned int */
  static __inline const char *decode8u(const char *p, u8 *c)
  {
    *c = *(u8*)p++;
    return p;
  }

  /* encode 16 bits unsigned int (lsb) */
  static __inline char *encode16u(char *p, u16 w)
  {
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
    *(u8*)(p + 0) = (w & 255);
    *(u8*)(p + 1) = (w >> 8);
#else
    memcpy(p, &w, 2);
#endif
    p += 2;
    return p;
  }

  /* decode 16 bits unsigned int (lsb) */
  static __inline const char *decode16u(const char *p, u16 *w)
  {
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
    *w = *(const u8*)(p + 1);
    *w = *(const u8*)(p + 0) + (*w << 8);
#else
    memcpy(w, p, 2);
#endif
    p += 2;
    return p;
  }

  /* encode 32 bits unsigned int (lsb) */
  static __inline char *encode32u(char *p, u32 l)
  {
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
    *(u8*)(p + 0) = (u8)((l >>  0) & 0xff);
    *(u8*)(p + 1) = (u8)((l >>  8) & 0xff);
    *(u8*)(p + 2) = (u8)((l >> 16) & 0xff);
    *(u8*)(p + 3) = (u8)((l >> 24) & 0xff);
#else
    memcpy(p, &l, 4);
#endif
    p += 4;
    return p;
  }

  /* decode 32 bits unsigned int (lsb) */
  static __inline const char *decode32u(const char *p, u32 *l)
  {
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
    *l = *(const u8*)(p + 3);
    *l = *(const u8*)(p + 2) + (*l << 8);
    *l = *(const u8*)(p + 1) + (*l << 8);
    *l = *(const u8*)(p + 0) + (*l << 8);
#else 
    memcpy(l, p, 4);
#endif
    p += 4;
    return p;
  }

  static __inline unsigned int hash32(const void* key, size_t len)
  {
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    const int seed = 97;
    unsigned int h = seed ^ (int)len;

    const unsigned char* data = (const unsigned char*)key;
    while (len >= 4)
    {
      unsigned int k = *(unsigned int*)data;
      k *= m;
      k ^= k >> r;
      k *= m;
      h *= m;
      h ^= k;
      data += 4;
      len -= 4;
    }

    switch (len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
      h *= m;
    };
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
  }

  //---------------------------------------------------------------------

  class decoder final
  {
  public:
    struct header {
      char fin;
      char rsv1, rsv2, rsv3;
      char hash, opcode;
      u16  cnt;
      u32  size;
    };

    inline decoder() {
      clear();

    }
    inline size_t size() const {
      return _buf.size();
    }

    inline const char* data() {
      return _buf.data();
    }

    inline size_t write(const char* data, size_t size) {
      return _buf.write(data, size);
    }

    inline void clear()
    {
      _buf.clear();
      _cache.clear();
      memset(&_head, 0, sizeof(_head));
    }

    template <typename Handler> int decode(Handler handler)
    {
      size_t size = _buf.size();
      const char* data = _buf.data();
      u8 byte1 = 0, byte2 = 0;

      while (size > 1)
      {
        const char* begin = data;
        size -= 2;
        data = decode8u(data, &byte1);
        data = decode8u(data, &byte2);

        _head.cnt++;
        _head.fin      = (byte1 >> 7);
        _head.rsv1     = (byte1 & 0x40) >> 6;
        _head.rsv2     = (byte1 & 0x20) >> 5;
        _head.rsv3     = (byte1 & 0x10) >> 4;
        _head.hash     = (byte2 >> 7);
        _head.opcode   = (byte1 & 0x0f);

        if (_head.rsv3) {
          return 1;
        }

        u16 length = (byte2 & 0x7f);
        if (length == 127)
        {
          if (size < 2) {
            break;
          }
          size -= 2;
          data = decode16u(data, &length);
        }

        u32 hash = 0;
        if (_head.hash)
        {
          if (size < 4) {
            break;
          }
          size -= 4;
          data = decode32u(data, &hash);
        }

        if (size < length) {
          break;
        }

        if (_head.rsv1) {
          _rc4.convert(
            data, length, (char*)data
          );
        }

        if (_head.hash){
          if (hash32(data, length) != hash) {
            return 2;
          }
        }

        if (_head.fin == 0 || _cache.size()) {
          _cache.write(data, length);
        }

        _head.size += length;
        if (_head.fin > 0)
        {
          const char* packet = data;
          if (!_cache.empty()) {
            packet = _cache.data();
          }

          char* buff = 0;
          if (_head.rsv2)
          {
            size_t n = qlz_size_compressed(packet);
            if (n != _head.size) {
              return 3;
            }

            n = qlz_size_decompressed(packet);
            buff = (char*)malloc(n);
            if (!buff) {
              return 4;
            }

            size_t x = qlz_decompress(packet, buff, &st);
            if (x != n) {
              free(buff);
              return 5;
            }

            packet = buff;
            _head.size = (u32)n;
          }

          handler(packet, _head.size, &_head);
          if (buff) {
            free(buff);
          }

          _cache.clear();
          memset(&_head, 0, sizeof(_head));
        }

        size = size - length;
        data = data + length;
        _buf.erase(data - begin);
      }
      return 0;
    }

  private:
    header _head;           //head of packet
    rc4_encoder     _rc4;   //rc4 encoder
    circular_buffer _buf;   //data received
    circular_buffer _cache; //packet of decoded
    qlz_state_decompress st;
  };

  //---------------------------------------------------------------------

  class encoder final {
    rc4_encoder _rc4;       //rc4 encoder
    qlz_state_compress st;

  public:
    template <typename Handler>
    void encode(int opcode, const char* data, size_t size, bool encrypt, bool compress, Handler handler)
    {
      assert(opcode < 0x10);
      char cache[8192];
      char* buff = 0;

      if (compress)
      {
        if (size + 400 < sizeof(cache)) {
          buff = cache;
        }
        else {
          buff = (char*)malloc(size + 400);
        }

        if (!buff) {
          compress = false;
        }
        else {
          size = qlz_compress(data, buff, size, &st);
          data = buff;
        }
      }

      u32 hash = 1;
      opcode &= 0x0f;
      circular_buffer buf;

      do
      {
        size_t len = _min_size(size, 0xffff);
        u8 fin = (size -= len) ? 0 : 0x80;

        u8 byte1 = fin | (u8)opcode;
        if (encrypt) {
          byte1 |= (1 << 6); //set rsv1 = 1
        }

        if (compress) {
          byte1 |= (1 << 5); //set rsv2 = 1
        }
        u8 byte2 = hash ? 0x80 : 0;

        buf.clear();
        buf.write((char*)&byte1, 1);

        if (len < 127)
        {
          byte2 |= (u8)len;
          buf.write((char*)&byte2, 1);
        }
        else
        {
          byte2 |= (u8)127;
          buf.write((char*)&byte2, 1);

          u16 nv = 0;
          encode16u((char*)&nv, (u16)len);
          buf.write((char*)&nv, 2);
        }

        if (hash)
        {
          u32 nv = 0;
          hash = hash32(data, len);
          encode32u((char*)&nv, (u32)hash);
          buf.write((char*)&nv, sizeof(nv));
        }

        buf.write(data, len);
        data = data + len;
        size_t n = buf.size() - len;

        const char* packet = buf.data() + n;
        if (encrypt) {
          _rc4.convert(packet, len, (char*)packet);
        }

        handler(buf.data(), buf.size());
      }
      while (size > 0);

      if (buff && buff != cache) free(buff);
    }
  };
}

/*******************************************************************************/

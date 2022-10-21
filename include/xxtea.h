
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

#define MX ((((z>>5)^(y<<2)) + ((y>>3)^(z<<4))) ^ ((sum^y) + (k[(p&3)^e]^z)))

/***********************************************************************************/

class xxtea final
{
  static void convert(int* v, int n, const int* k, int* out)
  {
    int p, q;
    unsigned int z, y = v[0];
    unsigned int sum = 0;
    unsigned int e, d = 0x9e3779b9;

    if (n > 1)  /* encrypt */
    {
      z = v[n - 1];
      q = 6 + 52 / n;
      while (q-- > 0)
      {
        sum += d; e = (sum >> 2) & 3;
        for (p = 0; p < n - 1; ++p)
        {
          y = v[p + 1];
          z = out[p] = v[p] + MX;
        }
        y = out[0];
        z = out[n - 1] = v[n - 1] + MX;
        v = out;
      }
    }
    else if (n < -1)  /* decrypt */
    {
      n = -n;
      z = v[n - 1];
      q = 6 + 52 / n; sum = q * d;

      while (sum != 0)
      {
        e = (sum >> 2) & 3;
        for (p = n - 1; p > 0; --p)
        {
          z = v[p - 1];
          y = out[p] = v[p] - MX;
        }
        z = out[n - 1];
        y = out[0] = v[0] - MX;
        sum -= d;
        v = out;
      }
    }
  }

public:
  static int encrypt(const char* v, int n, char* out, int* k = 0)
  {
    static const unsigned int key[4] = {
      0x41b23ff9, 0x428c04ca, 0xd23bfd6b, 0xd3e7291b
    };
    if (!k) {
      k = (int*)key;
    }
    int c = (n >> 2) + ((n & 3) ? 1 : 0);
    convert((int*)v, c, (const int*)key, (int*)out);
    return (c << 2);
  }

  static int decrypt(const char* v, int n, char* out, int* k = 0)
  {
    static const unsigned int key[4] = {
      0x41b23ff9, 0x428c04ca, 0xd23bfd6b, 0xd3e7291b
    };
    if (!k) {
      k = (int*)key;
    }
    int c = (n & 3) ? 0 : (n >> 2);
    convert((int*)v, -c, (const int*)key, (int*)out);
    return (c << 2);
  }
};

/***********************************************************************************/

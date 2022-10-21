

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

class rc4_encoder final {
    unsigned char s[256];
    unsigned int i, j, k;

public:
    inline rc4_encoder(const void *key, size_t bytes){
        reset(key, bytes);
    }
    inline rc4_encoder(){
        reset("374315ed9864f687d6d5144167944eb8", 32);
    }
    inline void reset(const void *key, size_t bytes)
    {
        unsigned char *p = (unsigned char*)key;
        for (k = 0, i = 1; i < bytes; i++)
        {
            k += (p[i] + p[i - 1]) & 255;
        }
        unsigned char c[256];
        for (i = 0; i < 256; i++)
        {
            s[i] = i;
            c[i] = p[i % bytes] ^ rand_c();
        }
        for (i = 0, j = 0; i < sizeof(s); i++)
        {
            j = (j + s[i] + c[i]) & 255;
            switch_c(i, j);
        }
        i = j = 0;
    }
    inline void convert(const void *in, size_t bytes, void *out)
    {
        unsigned char *p = (unsigned char*)in;
        unsigned char *v = (unsigned char*)out;
        for (size_t n = 0; n < bytes; n++)
        {
            i = (i + 1) & 255;
            j = (j + s[i]) & 255;
            switch_c(i, j);
            v[n] = p[n] ^ s[(s[i] + s[j]) & 255];
        }
    }
private:
    inline void switch_c(int a, int b) {
        unsigned char c = s[a];
        s[a] = s[b];
        s[b] = c;
    }
    inline unsigned char rand_c() {
        k = ((k * 0x343fd + 0x269ec3) >> 16) & 0x7fff;
        return (unsigned char)(k % sizeof(s));
    }
};

/***********************************************************************************/

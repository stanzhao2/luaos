/*
A C-program for MT19937, with initialization improved 2002/1/26.
Coded by Takuji Nishimura and Makoto Matsumoto.

Before using, initialize the state by using init_genrand(seed)
or init_by_array(init_key, key_length).

Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. The names of its contributors may not be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Any feedback is very welcome.
http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

#include <stdio.h>

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

typedef struct {
  /* the array for the state vector  */
  unsigned int mt[N];
  /* mti==N+1 means mt[N] is not initialized */
  int mti;
} gen_ctx32;

/* initializes mt[N] with a seed */
static void init_genrand(gen_ctx32* gen, unsigned int s)
{
  gen->mt[0] = s & 0xffffffffUL;
  for (gen->mti = 1; gen->mti < N; gen->mti++) {
    gen->mt[gen->mti] = (1812433253UL * (gen->mt[gen->mti - 1] ^ (gen->mt[gen->mti - 1] >> 30)) + gen->mti);
    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
    /* In the previous versions, MSBs of the seed affect   */
    /* only MSBs of the array mt[].                        */
    /* 2002/01/09 modified by Makoto Matsumoto             */
    gen->mt[gen->mti] &= 0xffffffffUL;
    /* for >32 bit machines */
  }
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
static void init_by_array(gen_ctx32* gen, unsigned int init_key[], int key_length)
{
  int i, j, k;
  init_genrand(gen, 19650218UL);
  i = 1; j = 0;
  k = (N > key_length ? N : key_length);
  for (; k; k--) {
    gen->mt[i] = (gen->mt[i] ^ ((gen->mt[i - 1] ^ (gen->mt[i - 1] >> 30)) * 1664525UL))
      + init_key[j] + j; /* non linear */
    gen->mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
    i++; j++;
    if (i >= N) { gen->mt[0] = gen->mt[N - 1]; i = 1; }
    if (j >= key_length) j = 0;
  }
  for (k = N - 1; k; k--) {
    gen->mt[i] = (gen->mt[i] ^ ((gen->mt[i - 1] ^ (gen->mt[i - 1] >> 30)) * 1566083941UL))
      - i; /* non linear */
    gen->mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
    i++;
    if (i >= N) { gen->mt[0] = gen->mt[N - 1]; i = 1; }
  }
  gen->mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0,0xffffffff]-interval */
static unsigned int genrand_int32(gen_ctx32* gen)
{
  unsigned int y;
  static unsigned int mag01[2] = { 0x0UL, MATRIX_A };
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (gen->mti >= N) { /* generate N words at one time */
    int kk;

    if (gen->mti == N + 1)   /* if init_genrand() has not been called, */
      init_genrand(gen, 5489UL); /* a default initial seed is used */

    for (kk = 0; kk < N - M; kk++) {
      y = (gen->mt[kk] & UPPER_MASK) | (gen->mt[kk + 1] & LOWER_MASK);
      gen->mt[kk] = gen->mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (; kk < N - 1; kk++) {
      y = (gen->mt[kk] & UPPER_MASK) | (gen->mt[kk + 1] & LOWER_MASK);
      gen->mt[kk] = gen->mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (gen->mt[N - 1] & UPPER_MASK) | (gen->mt[0] & LOWER_MASK);
    gen->mt[N - 1] = gen->mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
    gen->mti = 0;
  }

  y = gen->mt[gen->mti++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680UL;
  y ^= (y << 15) & 0xefc60000UL;
  y ^= (y >> 18);

  return y;
}

/* generates a random number on [0,0x7fffffff]-interval */
static int genrand_int31(gen_ctx32* gen)
{
  return (int)(genrand_int32(gen) >> 1);
}

/* generates a random number on [0,1]-real-interval */
static double genrand_real1(gen_ctx32* gen)
{
  return genrand_int32(gen) * (1.0 / 4294967295.0);
  /* divided by 2^32-1 */
}

/* generates a random number on [0,1)-real-interval */
static double genrand_real2(gen_ctx32* gen)
{
  return genrand_int32(gen) * (1.0 / 4294967296.0);
  /* divided by 2^32 */
}

/* generates a random number on (0,1)-real-interval */
static double genrand_real3(gen_ctx32* gen)
{
  return (((double)genrand_int32(gen)) + 0.5) * (1.0 / 4294967296.0);
  /* divided by 2^32 */
}

/* generates a random number on [0,1) with 53-bit resolution*/
static double genrand_res53(gen_ctx32* gen)
{
  unsigned int a = genrand_int32(gen) >> 5, b = genrand_int32(gen) >> 6;
  return(a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
}

static unsigned int roundup_power_of_2(unsigned int n)
{
  unsigned int maxv = ~0;
  unsigned int andv = ~(maxv & (maxv >> 1));
  while ((andv & n) == 0) andv = andv >> 1;
  return (andv << 1);
}

static unsigned int project(unsigned int ran, unsigned int n, void* gen)
{
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    unsigned int lim = roundup_power_of_2(n) - 1;
    while ((ran &= lim) > n)  /* project 'ran' into [0..lim] */
      ran = genrand_int31((gen_ctx32*)gen);  /* not inside [0..n]? try again */
    return ran;
  }
}

////////////////////////////////////////////////////////////////////////////////////

void* MT19937_state32()
{
  gen_ctx32* gen = new gen_ctx32();
  gen->mti = N + 1;
  return gen;
}

void MT19937_close32(void* gen)
{
  delete (gen_ctx32*)gen;
}

void MT19937_srand32(void* gen, unsigned int seed)
{
  init_genrand((gen_ctx32*)gen, seed);
}

unsigned int MT19937_random32(void* gen)
{
  return genrand_int31((gen_ctx32*)gen);
}

unsigned int MT19937_random32_range(void* gen, unsigned int low, unsigned int up)
{
  return low + project(genrand_int31((gen_ctx32*)gen), up - low, gen);
}

////////////////////////////////////////////////////////////////////////////////////

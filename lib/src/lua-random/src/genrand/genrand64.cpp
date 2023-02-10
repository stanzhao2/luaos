/*
A C-program for MT19937-64 (2004/9/29 version).
Coded by Takuji Nishimura and Makoto Matsumoto.

This is a 64-bit version of Mersenne Twister pseudorandom number
generator.

Before using, initialize the state by using init_genrand64(seed)
or init_by_array64(init_key, key_length).

Copyright (C) 2004, Makoto Matsumoto and Takuji Nishimura,
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

References:
T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
ACM Transactions on Modeling and
Computer Simulation 10. (2000) 348--357.
M. Matsumoto and T. Nishimura,
``Mersenne Twister: a 623-dimensionally equidistributed
uniform pseudorandom number generator''
ACM Transactions on Modeling and
Computer Simulation 8. (Jan. 1998) 3--30.

Any feedback is very welcome.
http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)
*/


#include <stdio.h>

#define NN 312
#define MM 156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define LM 0x7FFFFFFFULL /* Least significant 31 bits */

typedef struct {
  /* The array for the state vector */
  unsigned long long mt[NN];
  /* mti==NN+1 means mt[NN] is not initialized */
  int mti;
} gen_ctx64;

/* initializes mt[NN] with a seed */
static void init_genrand64(gen_ctx64* gen, unsigned long long seed)
{
  gen->mt[0] = seed;
  for (gen->mti = 1; gen->mti < NN; gen->mti++)
    gen->mt[gen->mti] = (6364136223846793005ULL * (gen->mt[gen->mti - 1] ^ (gen->mt[gen->mti - 1] >> 62)) + gen->mti);
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
static void init_by_array64(gen_ctx64* gen, unsigned long long init_key[], unsigned long long key_length)
{
  unsigned long long i, j, k;
  init_genrand64(gen, 19650218ULL);
  i = 1; j = 0;
  k = (NN > key_length ? NN : key_length);
  for (; k; k--) {
    gen->mt[i] = (gen->mt[i] ^ ((gen->mt[i - 1] ^ (gen->mt[i - 1] >> 62)) * 3935559000370003845ULL)) + init_key[j] + j; /* non linear */
    i++; j++;
    if (i >= NN) { gen->mt[0] = gen->mt[NN - 1]; i = 1; }
    if (j >= key_length) j = 0;
  }
  for (k = NN - 1; k; k--) {
    gen->mt[i] = (gen->mt[i] ^ ((gen->mt[i - 1] ^ (gen->mt[i - 1] >> 62)) * 2862933555777941757ULL))
      - i; /* non linear */
    i++;
    if (i >= NN) { gen->mt[0] = gen->mt[NN - 1]; i = 1; }
  }
  gen->mt[0] = 1ULL << 63; /* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0, 2^64-1]-interval */
static unsigned long long genrand64_int64(gen_ctx64* gen)
{
  int i;
  unsigned long long x;
  static unsigned long long mag01[2] = { 0ULL, MATRIX_A };

  if (gen->mti >= NN) { /* generate NN words at one time */
                   /* if init_genrand64() has not been called, */
                   /* a default initial seed is used     */
    if (gen->mti == NN + 1)
      init_genrand64(gen, 5489ULL);

    for (i = 0; i < NN - MM; i++) {
      x = (gen->mt[i] & UM) | (gen->mt[i + 1] & LM);
      gen->mt[i] = gen->mt[i + MM] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
    }
    for (; i < NN - 1; i++) {
      x = (gen->mt[i] & UM) | (gen->mt[i + 1] & LM);
      gen->mt[i] = gen->mt[i + (MM - NN)] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
    }
    x = (gen->mt[NN - 1] & UM) | (gen->mt[0] & LM);
    gen->mt[NN - 1] = gen->mt[MM - 1] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
    gen->mti = 0;
  }

  x = gen->mt[gen->mti++];

  x ^= (x >> 29) & 0x5555555555555555ULL;
  x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
  x ^= (x << 37) & 0xFFF7EEE000000000ULL;
  x ^= (x >> 43);

  return x;
}

/* generates a random number on [0, 2^63-1]-interval */
static long long genrand64_int63(gen_ctx64* gen)
{
  return (long long)(genrand64_int64(gen) >> 1);
}

/* generates a random number on [0,1]-real-interval */
static double genrand64_real1(gen_ctx64* gen)
{
  return (genrand64_int64(gen) >> 11) * (1.0 / 9007199254740991.0);
}

/* generates a random number on [0,1)-real-interval */
static double genrand64_real2(gen_ctx64* gen)
{
  return (genrand64_int64(gen) >> 11) * (1.0 / 9007199254740992.0);
}

/* generates a random number on (0,1)-real-interval */
static double genrand64_real3(gen_ctx64* gen)
{
  return ((genrand64_int64(gen) >> 12) + 0.5) * (1.0 / 4503599627370496.0);
}

static unsigned long long roundup_power_of_2(unsigned long long n)
{
  unsigned long long maxv = ~0;
  unsigned long long andv = ~(maxv & (maxv >> 1));
  while ((andv & n) == 0) andv = andv >> 1;
  return (andv << 1);
}

static unsigned long long project(unsigned long long ran, unsigned long long n, void* gen)
{
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    unsigned long long lim = roundup_power_of_2(n) - 1;
    while ((ran &= lim) > n) { /* project 'ran' into [0..lim] */
      ran = genrand64_int63((gen_ctx64*)gen);  /* not inside [0..n]? try again */
    }
    return ran;
  }
}

////////////////////////////////////////////////////////////////////////////////////

void* MT19937_state64()
{
  gen_ctx64* gen = new gen_ctx64();
  gen->mti = NN + 1;
  return gen;
}

void MT19937_close64(void* gen)
{
  delete (gen_ctx64*)gen;
}

void MT19937_srand64(void* gen, unsigned long long seed)
{
  init_genrand64((gen_ctx64*)gen, seed);
}

unsigned long long MT19937_random64(void* gen)
{
  return genrand64_int63((gen_ctx64*)gen);
}

unsigned long long MT19937_random64_range(void* gen, unsigned long long low, unsigned long long up)
{
  return low + project(genrand64_int63((gen_ctx64*)gen), up - low, gen);
}

////////////////////////////////////////////////////////////////////////////////////

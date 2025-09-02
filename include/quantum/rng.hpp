
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace qsx {
// Minimal PCG32 RNG (portable, reproducible across compilers/OS)
struct Pcg32 {
  uint64_t state;
  uint64_t inc;
  explicit Pcg32(uint64_t seed=0x853c49e6748fea9bULL, uint64_t seq=0xda3e39cb94b95bdbULL){ seed_rng(seed, seq); }
  void seed_rng(uint64_t seed, uint64_t seq){
    state = 0U; inc = (seq << 1u) | 1u;
    next(); state += seed; next();
  }
  uint32_t next(){
    uint64_t oldstate = state;
    state = oldstate * 6364136223846793005ULL + inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }
  uint32_t operator()(){ return next(); }
  double uniform01(){ return (next() >> 8) * (1.0/9007199254740992.0); } // 53-bit fraction
  uint32_t randint(uint32_t n){ return n? (next() % n) : 0; }
};
} // namespace qsx

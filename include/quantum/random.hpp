// SPDX-License-Identifier: MIT

#pragma once
#include <random>
#include <cstdint>

namespace qsx {
class Rng {
  std::mt19937_64 gen;
  std::uniform_real_distribution<double> dist;
public:
  explicit Rng(uint64_t seed) : gen(seed), dist(0.0, 1.0) {}
  double uniform() { return dist(gen); }
};
}

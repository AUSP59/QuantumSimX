// SPDX-License-Identifier: MIT

#pragma once
#include "types.hpp"
#include "random.hpp"
#include <vector>

namespace qsx {

class DensityMatrix {
  std::size_t n_;
  std::vector<c64> rho_; // row-major 2^n x 2^n
  void renormalize_();
public:
  explicit DensityMatrix(std::size_t n);
  std::size_t num_qubits() const { return n_; }
  std::size_t dim() const { return (std::size_t(1) << n_); }
  const std::vector<c64>& data() const { return rho_; }

  void apply_unitary_1q(std::size_t target, const c64 u00, const c64 u01, const c64 u10, const c64 u11);
  void apply_cx(std::size_t control, std::size_t target);

  // Noise channels via Kraus operators
  void dephase(std::size_t target, double p);
  void depolarize(std::size_t target, double p);
};

struct DMRunResult {
  std::vector<int> outcome;
  std::vector<double> probabilities;
};

DMRunResult run_density(const Circuit& c, uint64_t seed, bool collapse);

} // namespace qsx

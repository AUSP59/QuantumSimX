// SPDX-License-Identifier: MIT

#pragma once
#include "types.hpp"
#include "random.hpp"
#include <span>
#include <optional>

namespace qsx {

class StateVector {
  std::size_t n_;
  vec_c64 amp_;
  std::size_t applied_ = 0;
  void normalize_();

public:
  explicit StateVector(std::size_t n);
  std::size_t num_qubits() const { return n_; }
  std::size_t dimension() const { return amp_.size(); }
  const vec_c64& amplitudes() const { return amp_; }
  vec_c64& amplitudes_mut() { return amp_; }
  bool save(const std::string& path) const;
  static std::optional<StateVector> load(const std::string& path, std::size_t n_expected);

  // Single-qubit 2x2 gate on target qubit (0-indexed, LSB = qubit 0)
  void apply_gate_1q(std::size_t target, const c64 u00, const c64 u01, const c64 u10, const c64 u11);

  // Controlled single-qubit gate with one control (control must be 1).
  void apply_cx(std::size_t control, std::size_t target); // CNOT
  void apply_controlled_1q(std::size_t control, std::size_t target, const c64 u00, const c64 u01, const c64 u10, const c64 u11);

  // Measurement: returns bitstring outcome and optionally collapse.
  std::vector<int> measure_all(Rng& rng, bool collapse=true);
  double probability_of_basis(std::size_t basis_index) const;
};

} // namespace qsx

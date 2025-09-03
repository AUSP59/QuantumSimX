// SPDX-License-Identifier: MIT

#pragma once
#include "circuit.hpp"
#include <vector>
#include <optional>

namespace qsx {

struct GradResult {
  // grads[param_index][qubit] = d <Z_q> / d theta_param
  std::vector<std::vector<double>> grads;
  std::vector<std::size_t> param_op_indices;
};

// Compute gradients via parameter-shift (state backend), considering RX/RY/RZ only.
// If wrt_indices is empty, all parameterized ops are used.
std::optional<GradResult> grad_expZ_parameter_shift(const Circuit& c, const std::vector<std::size_t>& wrt_indices, uint64_t seed);

} // namespace qsx

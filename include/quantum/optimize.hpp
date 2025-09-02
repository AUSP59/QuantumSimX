// SPDX-License-Identifier: MIT

#pragma once
#include "circuit.hpp"
#include <utility>

namespace qsx {

struct OptimizeOptions {
  bool fuse_single_qubit = true;
  bool cancel_involutory = true; // X^2=I, H^2=I, Z^2=I, S^4=I (also S^2=Z)
  bool merge_rotations = true;   // RX/RY/RZ on same target sum angles
  bool cancel_cnot_pairs = true; // consecutive identical CNOT pairs
};

Circuit optimize(const Circuit& in, OptimizeOptions opts = {});

} // namespace qsx

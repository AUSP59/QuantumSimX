// SPDX-License-Identifier: MIT

#pragma once
#include "state_vector.hpp"
#include "gates.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace qsx {

enum class OpType { H, X, Y, Z, S, RX, RY, RZ, CNOT, MEASURE, DEPHASE, DEPOL, AMPDAMP };

struct Op {
  OpType type;
  std::vector<std::size_t> qubits;
  double angle = 0.0; // for rotations
};

struct Circuit {
  std::size_t nqubits{};
  std::vector<Op> ops;
};

// Parse a simple .qsx circuit file.
// Lines:
//   H 0
//   X 1
//   RZ 0 1.57079632679
//   CNOT 0 1
//   MEASURE ALL
std::optional<Circuit> parse_circuit_file(const std::string& path, std::string& err);

// Execute circuit
struct RunResult {
  std::vector<int> outcome;
  std::vector<double> probabilities; // size 2^n
};

RunResult run(const Circuit& c, uint64_t seed, bool collapse=true);

} // namespace qsx

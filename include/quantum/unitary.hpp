// SPDX-License-Identifier: MIT

#pragma once
#include "circuit.hpp"
#include <vector>

namespace qsx {

// Build full unitary matrix (2^n x 2^n) for a circuit composed of unitary ops.
// Supports H, X, Y, Z, S, RX, RY, RZ, CNOT. Fails if noise or MEASURE present.
// Returns row-major vector of complex numbers (size d*d).
std::vector<c64> build_unitary(const Circuit& c);

// Export unitary to CSV (real,imag pairs per cell)
bool export_unitary_csv(const Circuit& c, const std::string& path);

} // namespace qsx

// SPDX-License-Identifier: MIT

#pragma once
#include "circuit.hpp"
#include <optional>
#include <string>

namespace qsx {
// Minimal OpenQASM 2.0 subset: qreg, h, x, y, z, s, rx, ry, rz, cx, measure (ignored except MEASURE ALL)
std::optional<Circuit> parse_qasm_file(const std::string& path, std::string& err);
}

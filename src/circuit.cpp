// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include <fstream>
#include <sstream>
#include <charconv>
#include <cctype>

namespace qsx {

static bool parse_size_t(const std::string& s, std::size_t& out) {
  try {
    std::size_t pos=0;
    unsigned long long v = std::stoull(s, &pos, 10);
    if (pos != s.size()) return false;
    out = static_cast<std::size_t>(v);
    return true;
  } catch(...) { return false; }
}

std::optional<Circuit> parse_circuit_file(const std::string& path, std::string& err) {
  std::ifstream in(path);
  if (!in) { err = "Cannot open circuit file: " + path; return std::nullopt; }
  Circuit c;
  std::string line;
  std::size_t lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    // strip comments (# ...)
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    // trim
    auto notspace = [](unsigned char ch){ return !std::isspace(ch); };
    auto l = std::find_if(line.begin(), line.end(), notspace);
    auto r = std::find_if(line.rbegin(), line.rend(), notspace).base();
    if (l >= r) continue;
    std::istringstream ss(std::string(l, r));
    std::string op;
    ss >> op;
    if (op == "H" || op == "X" || op == "Y" || op == "Z" || op == "S") {
      std::string tq; ss >> tq;
      std::size_t t;
      if (!parse_size_t(tq, t)) { err = "Invalid target at line " + std::to_string(lineno); return std::nullopt; }
      c.ops.push_back({op=="H"?OpType::H:op=="X"?OpType::X:op=="Y"?OpType::Y:op=="Z"?OpType::Z:OpType::S, {t}, 0.0});
      c.nqubits = std::max(c.nqubits, t+1);

} else if (op == "RX" || op == "RY") {
  std::string tq, ang; ss >> tq >> ang;
  std::size_t t; if (!parse_size_t(tq, t)) { err = "Invalid target at line " + std::to_string(lineno); return std::nullopt; }
  double a = std::stod(ang);
  c.ops.push_back({op=="RX"?OpType::RX:OpType::RY, {t}, a});
  c.nqubits = std::max(c.nqubits, t+1);
    } else if (op == "RZ") {
      std::string tq, ang; ss >> tq >> ang;
      std::size_t t; if (!parse_size_t(tq, t)) { err = "Invalid target at line " + std::to_string(lineno); return std::nullopt; }
      double a = std::stod(ang);
      c.ops.push_back({OpType::RZ, {t}, a});
      c.nqubits = std::max(c.nqubits, t+1);

} else if (op == "DEPHASE") {
  std::string tq, prob; ss >> tq >> prob;
  std::size_t t; if (!parse_size_t(tq, t)) { err = "Invalid target at line " + std::to_string(lineno); return std::nullopt; }
  double p = std::stod(prob);
  if (p < 0.0 || p > 1.0) { err = "Probability out of range at line " + std::to_string(lineno); return std::nullopt; }
  c.ops.push_back({OpType::DEPHASE, {t}, p});
  c.nqubits = std::max(c.nqubits, t+1);
} else if (op == "DEPOL") {
  std::string tq, prob; ss >> tq >> prob;
  std::size_t t; if (!parse_size_t(tq, t)) { err = "Invalid target at line " + std::to_string(lineno); return std::nullopt; }
  double p = std::stod(prob);
  if (p < 0.0 || p > 1.0) { err = "Probability out of range at line " + std::to_string(lineno); return std::nullopt; }
  c.ops.push_back({OpType::DEPOL, {t}, p});
  c.nqubits = std::max(c.nqubits, t+1);
    } else if (op == "CNOT") {
      std::string cq, tq; ss >> cq >> tq;
      std::size_t cbit, tbit;
      if (!parse_size_t(cq, cbit) || !parse_size_t(tq, tbit)) { err = "Invalid CNOT at line " + std::to_string(lineno); return std::nullopt; }
      c.ops.push_back({OpType::CNOT, {cbit, tbit}, 0.0});
      c.nqubits = std::max(c.nqubits, std::max(cbit, tbit)+1);
    } else if (op == "MEASURE") {
      std::string all; ss >> all;
      if (all != "ALL") { err = "Only 'MEASURE ALL' supported at line " + std::to_string(lineno); return std::nullopt; }
      c.ops.push_back({OpType::MEASURE, {}, 0.0});
    } else {
      err = "Unknown op '" + op + "' at line " + std::to_string(lineno);
      return std::nullopt;
    }
  }
  return c;
}


RunResult run(const Circuit& c, uint64_t seed, bool collapse) {
  StateVector sv(c.nqubits);
  Rng rng(seed);
  // Apply ops
  for (const auto& op : c.ops) {
    using namespace qsx::gates;
    c64 u00,u01,u10,u11;
    switch (op.type) {
      case OpType::H:
        H_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::X:
        X_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::Y:
        Y_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::Z:
        Z_coeffs(u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::S:
        S_coeffs(u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::RX:
        RX_coeffs(op.angle, u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::RY:
        RY_coeffs(op.angle, u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::Z:
        Z_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::RZ:
        RZ_coeffs(op.angle, u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::CNOT:
        sv.apply_cx(op.qubits[0], op.qubits[1]);
        break;
      case OpType::DEPHASE: {
        // Simple dephasing: with prob p apply Z, else I
        double r = rng.uniform();
        if (r < op.angle) { // angle stores probability
          Z_coeffs(u00,u01,u10,u11);
          sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        }
        break;
      }
      case OpType::DEPOL: {
        // Depolarizing: with prob p apply uniformly random X/Y/Z
        double r = rng.uniform();
        if (r < op.angle) {
          double k = rng.uniform();
          if (k < 1.0/3.0) { X_coeffs(u00,u01,u10,u11); }
          else if (k < 2.0/3.0) { Y_coeffs(u00,u01,u10,u11); }
          else { Z_coeffs(u00,u01,u10,u11); }
          sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        }
        break;
      }
      case OpType::MEASURE:
        // handled after loop
        break;
    }
  }
  // Output probabilities
  RunResult rr;
  rr.probabilities.resize(std::size_t(1) << c.nqubits);
  for (std::size_t i=0;i<rr.probabilities.size();++i) rr.probabilities[i] = sv.probability_of_basis(i);
  // Measure
  rr.outcome = sv.measure_all(rng, collapse);
  return rr;
}

  StateVector sv(c.nqubits);
  // Apply ops
  for (const auto& op : c.ops) {
    using namespace qsx::gates;
    c64 u00,u01,u10,u11;
    switch (op.type) {
      case OpType::H:
        H_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::X:
        X_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::Z:
        Z_coeffs(u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::S:
        S_coeffs(u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::RX:
        RX_coeffs(op.angle, u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::RY:
        RY_coeffs(op.angle, u00,u01,u10,u11); sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11); break;
      case OpType::Z:
        Z_coeffs(u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::RZ:
        RZ_coeffs(op.angle, u00,u01,u10,u11);
        sv.apply_gate_1q(op.qubits[0], u00,u01,u10,u11);
        break;
      case OpType::CNOT:
        sv.apply_cx(op.qubits[0], op.qubits[1]);
        break;
      case OpType::MEASURE:
        // handled after loop
        break;
    }
  }
  // Output probabilities
  RunResult rr;
  rr.probabilities.resize(std::size_t(1) << c.nqubits);
  for (std::size_t i=0;i<rr.probabilities.size();++i) rr.probabilities[i] = sv.probability_of_basis(i);
  // Measure
  Rng rng(seed);
  rr.outcome = sv.measure_all(rng, collapse);
  return rr;
}

} // namespace qsx

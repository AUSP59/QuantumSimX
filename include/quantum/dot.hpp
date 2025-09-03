// SPDX-License-Identifier: MIT

#pragma once
#include "circuit.hpp"
#include <string>
#include <fstream>

namespace qsx {
inline bool export_dot(const Circuit& c, const std::string& path){
  std::ofstream out(path);
  if(!out) return false;
  out << "digraph circuit {\n  rankdir=LR;\n";
  for (std::size_t q=0; q<c.nqubits; ++q){
    out << "  q" << q << " [shape=plaintext,label="q" << q << ""];\n";
  }
  std::size_t idx=0;
  for (auto& op: c.ops){
    std::string name;
    switch(op.type){
      case OpType::H: name="H"; break; case OpType::X: name="X"; break; case OpType::Y: name="Y"; break; case OpType::Z: name="Z"; break;
      case OpType::S: name="S"; break; case OpType::RX: name="RX"; break; case OpType::RY: name="RY"; break; case OpType::RZ: name="RZ"; break;
      case OpType::CNOT: name="CNOT"; break; case OpType::MEASURE: name="MEASURE"; break; case OpType::DEPHASE: name="DEPHASE"; break;
      case OpType::DEPOL: name="DEPOL"; break; case OpType::AMPDAMP: name="AMPDAMP"; break;
    }
    out << "  n" << idx << " [shape=box,label="" << name << ""];\n";
    for (auto q : op.qubits){
      out << "  q" << q << " -> n" << idx << ";\n";
      out << "  n" << idx << " -> q" << q << ";\n";
    }
    ++idx;
  }
  out << "}\n";
  return bool(out);
}
} // namespace qsx

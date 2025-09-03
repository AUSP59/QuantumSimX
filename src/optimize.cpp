// SPDX-License-Identifier: MIT

#include "quantum/optimize.hpp"
#include <cmath>

namespace qsx {

static bool is_involutory(OpType t){
  return t==OpType::X || t==OpType::H || t==OpType::Z;
}

Circuit optimize(const Circuit& in, OptimizeOptions opts){
  Circuit out; out.nqubits = in.nqubits;
  out.ops.reserve(in.ops.size());
  // First pass: merge and cancel on the fly for single-qubit gates per target
  for (const auto& op : in.ops){
    if (op.type==OpType::MEASURE || op.type==OpType::CNOT || op.type==OpType::DEPHASE || op.type==OpType::DEPOL || op.type==OpType::AMPDAMP){
      // handle 2q and noise ops verbatim
      out.ops.push_back(op);
      continue;
    }
    // Single qubit ops
    if (out.ops.empty() || out.ops.back().qubits.size()!=1 || out.ops.back().qubits[0]!=op.qubits[0]){
      out.ops.push_back(op);
      continue;
    }
    auto &prev = out.ops.back();
    if (opts.merge_rotations){
      if ((prev.type==OpType::RZ && op.type==OpType::RZ) ||
          (prev.type==OpType::RX && op.type==OpType::RX) ||
          (prev.type==OpType::RY && op.type==OpType::RY)){
        prev.angle += op.angle;
        continue;
      }
    }
    if (opts.cancel_involutory && is_involutory(prev.type) && prev.type==op.type){
      out.ops.pop_back();
      continue;
    }
    // S rules
    if (opts.cancel_involutory && prev.type==OpType::S && op.type==OpType::S){
      // S*S = Z
      prev.type = OpType::Z; prev.angle = 0.0;
      continue;
    }
    out.ops.push_back(op);
  }
  // Second pass: cancel zero-angle rotations and consecutive identical CNOT pairs
  Circuit out2; out2.nqubits = out.nqubits;
  for (size_t i=0;i<out.ops.size();++i){
    const auto& op = out.ops[i];
    if ((op.type==OpType::RX || op.type==OpType::RY || op.type==OpType::RZ) && std::fabs(op.angle) < 1e-15) continue;
    if (opts.cancel_cnot_pairs && op.type==OpType::CNOT && i+1<out.ops.size()){
      const auto& op2 = out.ops[i+1];
      if (op2.type==OpType::CNOT && op.qubits==op2.qubits){
        ++i; continue; // cancel pair
      }
    }
    out2.ops.push_back(op);
  }
  return out2;
}

} // namespace qsx

// SPDX-License-Identifier: MIT

#pragma once
#include "circuit.hpp"
#include <vector>
#include <algorithm>

namespace qsx {

// Naive linear topology mapper: ensures all CNOTs act on adjacent qubits (|i-j|=1)
// by inserting SWAPs and maintaining a logical->physical map. Returns mapped circuit.
inline Circuit map_to_line(const Circuit& in){
  Circuit out; out.nqubits = in.nqubits;
  std::vector<std::size_t> phys(in.nqubits); // logical -> physical
  for (std::size_t i=0;i<in.nqubits;++i) phys[i]=i;
  auto emit_swap = [&](std::size_t a, std::size_t b){
    // swap physical neighbors a<->b via three CNOTs (control-target-control)
    out.ops.push_back({OpType::CNOT,{a,b},0.0});
    out.ops.push_back({OpType::CNOT,{b,a},0.0});
    out.ops.push_back({OpType::CNOT,{a,b},0.0});
  };
  for (const auto& op : in.ops){
    if (op.type == OpType::CNOT && op.qubits.size()==2){
      std::size_t lc = op.qubits[0], lt = op.qubits[1];
      std::size_t pc = phys[lc], pt = phys[lt];
      while (pc+1 < pt){
        emit_swap(pc, pc+1);
        // Update inverse map: find which logical is at pc and pc+1
        for (std::size_t l=0;l<phys.size();++l){
          if (phys[l]==pc) phys[l]=pc+1;
          else if (phys[l]==pc+1) phys[l]=pc;
        }
        ++pc;
      }
      while (pt+1 < pc){
        emit_swap(pt, pt+1);
        for (std::size_t l=0;l<phys.size();++l){
          if (phys[l]==pt) phys[l]=pt+1;
          else if (phys[l]==pt+1) phys[l]=pt;
        }
        ++pt;
      }
      // Now adjacent
      out.ops.push_back({OpType::CNOT,{pc,pt},0.0});
    } else if (op.qubits.size()==1){
      out.ops.push_back({op.type, { phys[op.qubits[0]] }, op.angle});
    } else if (op.type==OpType::MEASURE || op.type==OpType::DEPHASE || op.type==OpType::DEPOL || op.type==OpType::AMPDAMP){
      out.ops.push_back(op); // noise/measure unaffected (acts on logical indices equivalently here)
    } else {
      out.ops.push_back(op);
    }
  }
  return out;
}

} // namespace qsx

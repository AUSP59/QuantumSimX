// SPDX-License-Identifier: MIT

#include "quantum/map.hpp"
#include "quantum/circuit.hpp"
#include <cassert>
int main(){
  qsx::Circuit c; c.nqubits=3;
  c.ops.push_back({qsx::OpType::H,{0},0.0});
  c.ops.push_back({qsx::OpType::CNOT,{0,2},0.0}); // non-adjacent
  auto m = qsx::map_to_line(c);
  // Expect more ops after mapping due to SWAPs + CNOT
  assert(m.ops.size() >= c.ops.size());
  return 0;
}

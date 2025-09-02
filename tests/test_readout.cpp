// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include <cassert>
#include <iostream>

int main(){
  qsx::Circuit c; c.nqubits=1;
  // Identity circuit
  c.ops.push_back({qsx::OpType::MEASURE, {}, 0.0});
  auto r = qsx::run(c, 123, false);
  // Post-process readout flip is at CLI layer; here we just ensure measure returns a bit
  assert(r.outcome.size()==1);
  return 0;
}

// SPDX-License-Identifier: MIT

#include "quantum/unitary.hpp"
#include "quantum/circuit.hpp"
#include <cassert>
#include <cmath>
int main(){
  qsx::Circuit c; c.nqubits=1;
  c.ops.push_back({qsx::OpType::H,{0},0.0});
  auto U = qsx::build_unitary(c);
  // Check unitarity: U^† U = I
  std::complex<double> a=U[0], b=U[1], c1=U[2], d=U[3];
  // For H, entries are 1/sqrt(2)
  double s = std::norm(a)+std::norm(c1);
  assert(std::fabs(s - 1.0) < 1e-12);
  return 0;
}

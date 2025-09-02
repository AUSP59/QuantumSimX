// SPDX-License-Identifier: MIT
#include "quantum/grad.hpp"
#include <cmath>
#include <iostream>
using namespace qsx;
int main(){
  Circuit c; c.nqubits=1;
  c.ops.push_back({OpType::RY,{0}, M_PI/3});
  auto gr = grad_expZ_parameter_shift(c, {}, 123);
  if (!gr) return 1;
  double expected = -std::sin(M_PI/3);
  double got = gr->grads[0][0];
  if (std::fabs(got - expected) > 1e-6){
    std::cerr << "grad mismatch: " << got << " vs " << expected << "\n";
    return 2;
  }
  std::cout << "OK\n";
  return 0;
}

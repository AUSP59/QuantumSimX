// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include "quantum/density_matrix.hpp"
#include <cmath>
#include <iostream>

using namespace qsx;

static int fails=0;
#define CHECK_NEAR(a,b,e) do{ if (std::fabs((a)-(b))>(e)) { std::cerr << "Mismatch: " << (a) << " vs " << (b) << "\n"; ++fails; } }while(0)

int main(){
  Circuit c; c.nqubits=2;
  c.ops.push_back({OpType::H,{0},0.0});
  c.ops.push_back({OpType::CNOT,{0,1},0.0});
  // State-vector
  auto r1 = run(c, 123, false);
  // Density matrix (unitary only)
  auto r2 = run_density(c, 123, false);
  for (size_t i=0;i<r1.probabilities.size();++i) CHECK_NEAR(r1.probabilities[i], r2.probabilities[i], 1e-12);
  if (fails==0) std::cout << "OK\n";
  return fails==0?0:1;
}

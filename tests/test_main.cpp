// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include <iostream>
#include <cmath>

using namespace qsx;

static int tests_failed = 0;
#define EXPECT_NEAR(a,b,eps) do{ if (std::fabs((a)-(b))>(eps)) { std::cerr << "EXPECT_NEAR failed at " << __LINE__ << ": " << (a) << " vs " << (b) << "\n"; ++tests_failed; } }while(0)

int main(){
  // Bell state: H 0; CNOT 0 1;
  Circuit c; c.nqubits=2;
  c.ops.push_back({OpType::H,{0},0.0});
  c.ops.push_back({OpType::CNOT,{0,1},0.0});
  auto r = run(c, 1234, false);
  // Probabilities: 00 ~ 0.5, 11 ~ 0.5
  EXPECT_NEAR(r.probabilities[0], 0.5, 1e-9);
  EXPECT_NEAR(r.probabilities[3], 0.5, 1e-9);
  EXPECT_NEAR(r.probabilities[1], 0.0, 1e-12);
  EXPECT_NEAR(r.probabilities[2], 0.0, 1e-12);
  if (tests_failed==0){ std::cout << "OK\n"; }
  return tests_failed == 0 ? 0 : 1;
}

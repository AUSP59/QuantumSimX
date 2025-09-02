// SPDX-License-Identifier: MIT

#include "quantum/mitigate.hpp"
#include <vector>
#include <cassert>
int main(){
  std::vector<double> p = {0.9, 0.1};
  auto q = qsx::mitigate_readout(p, 1, 0.05, 0.02);
  double s=0; for(double v: q) s+=v; assert(std::abs(s-1.0) < 1e-9);
  return 0;
}

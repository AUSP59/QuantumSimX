// SPDX-License-Identifier: MIT
#include "quantum/optimize.hpp"
#include <cassert>
int main(){
  qsx::Circuit c; c.nqubits=1;
  c.ops.push_back({qsx::OpType::H,{0},0.0});
  c.ops.push_back({qsx::OpType::H,{0},0.0});
  auto o = qsx::optimize(c, {});
  assert(o.ops.size()==0);
  return 0;
}

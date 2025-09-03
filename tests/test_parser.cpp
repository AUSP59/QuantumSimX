// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include <cassert>
#include <string>

using namespace qsx;
int main(){
  std::string err;
  auto c = parse_circuit_file("examples/bell.qsx", err);
  assert(c.has_value());
  assert(c->nqubits>=2);
  return 0;
}

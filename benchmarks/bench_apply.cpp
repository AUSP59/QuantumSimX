// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include <chrono>
#include <iostream>

using namespace qsx;

int main(){
  int n=20;
  Circuit c; c.nqubits=n;
  for(int i=0;i<100;++i){ c.ops.push_back({OpType::H,{std::size_t(i% n)},0.0}); }
  for(int i=0;i<100;++i){ c.ops.push_back({OpType::CNOT,{std::size_t(i% (n-1)), std::size_t((i% (n-1))+1)},0.0}); }
  auto t0 = std::chrono::steady_clock::now();
  auto r = run(c, 42, false); (void)r;
  auto t1 = std::chrono::steady_clock::now();
  std::chrono::duration<double> dt = t1 - t0;
  std::cout << "Elapsed seconds: " << dt.count() << "\n";
  return 0;
}

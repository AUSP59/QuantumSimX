// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::string s(reinterpret_cast<const char*>(data), size);
  std::string path = "fuzz_tmp.qsx";
  std::ofstream out(path); out<<s; out.close();
  std::string err;
  auto c = qsx::parse_circuit_file(path, err);
  return 0;
}

// SPDX-License-Identifier: MIT

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "quantum/circuit.hpp"

namespace py = pybind11;
using namespace qsx;

PYBIND11_MODULE(qsx_python, m){
  m.def("run_qsx", [](const std::string& path, uint64_t seed){
    std::string err; auto c = parse_circuit_file(path, err);
    if (!c) throw std::runtime_error(err);
    auto r = run(*c, seed, false);
    return py::make_tuple(r.outcome, r.probabilities);
  });
}

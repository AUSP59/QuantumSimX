// SPDX-License-Identifier: MIT

#include "quantum/grad.hpp"
#include "quantum/circuit.hpp"
#include <cmath>

namespace qsx {

static std::vector<double> expZ_from_probs(const std::vector<double>& probs, std::size_t nqubits){
  std::vector<double> out(nqubits, 0.0);
  for (std::size_t q=0;q<nqubits;++q){
    double z=0.0;
    for (std::size_t i=0;i<probs.size();++i){
      int bit = (i>>q)&1;
      z += (bit? -probs[i] : probs[i]);
    }
    out[q]=z;
  }
  return out;
}

std::optional<GradResult> grad_expZ_parameter_shift(const Circuit& c, const std::vector<std::size_t>& wrt_indices, uint64_t seed){
  // Collect parameterized op indices
  std::vector<std::size_t> params;
  if (wrt_indices.empty()){
    for (std::size_t i=0;i<c.ops.size();++i){
      auto t = c.ops[i].type;
      if (t==OpType::RX || t==OpType::RY || t==OpType::RZ) params.push_back(i);
    }
  } else {
    params = wrt_indices;
  }
  GradResult gr; gr.param_op_indices = params;
  gr.grads.assign(params.size(), std::vector<double>(c.nqubits, 0.0));

  // Helper to run circuit with modified angle at given index
  auto run_probs = [&](std::size_t idx, double delta)->std::vector<double>{
    Circuit c2 = c;
    c2.ops[idx].angle += delta;
    auto rr = run(c2, seed, false);
    return rr.probabilities;
  };

  const double s = M_PI/2.0;
  for (std::size_t k=0;k<params.size();++k){
    std::size_t idx = params[k];
    auto p_plus  = run_probs(idx, +s);
    auto p_minus = run_probs(idx, -s);
    auto ez_plus  = expZ_from_probs(p_plus,  c.nqubits);
    auto ez_minus = expZ_from_probs(p_minus, c.nqubits);
    for (std::size_t q=0;q<c.nqubits;++q){
      gr.grads[k][q] = 0.5 * (ez_plus[q] - ez_minus[q]);
    }
  }
  return gr;
}

} // namespace qsx

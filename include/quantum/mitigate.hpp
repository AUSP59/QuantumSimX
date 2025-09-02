// SPDX-License-Identifier: MIT

#pragma once
#include <vector>
#include <cstddef>
#include "types.hpp"

namespace qsx {
// Apply M^{-1 ⊗ n} on probability vector p (size 2^n), where
// M = [[1-p01, p10],[p01, 1-p10]]. Returns mitigated probabilities (sum renormalized to 1).
inline std::vector<double> mitigate_readout(const std::vector<double>& p, std::size_t nqubits, double p01, double p10){
  double a = 1.0 - p01, b = p10, c = p01, d = 1.0 - p10;
  double det = a*d - b*c;
  if (det == 0.0) return p;
  double ia =  d / det, ib = -b / det, ic = -c / det, id =  a / det; // inverse of M
  std::vector<double> out = p;
  std::size_t dim = p.size();
  for (std::size_t q=0; q<nqubits; ++q){
    std::size_t step = std::size_t(1) << q;
    for (std::size_t base=0; base<dim; base += (step<<1)){
      for (std::size_t i=0; i<step; ++i){
        std::size_t i0 = base + i;
        std::size_t i1 = i0 + step;
        double x0 = out[i0], x1 = out[i1];
        out[i0] = ia*x0 + ib*x1;
        out[i1] = ic*x0 + id*x1;
      }
    }
  }
  // clip negatives from numeric noise and renormalize
  double s = 0.0;
  for (auto& v: out){ if (v < 0.0) v = 0.0; s += v; }
  if (s > 0) for (auto& v: out) v /= s;
  return out;
}
} // namespace qsx

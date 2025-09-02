// SPDX-License-Identifier: MIT

#include "quantum/density_matrix.hpp"
#include "quantum/circuit.hpp"
#include "quantum/gates.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>

namespace qsx {

static inline std::size_t idx(std::size_t row, std::size_t col, std::size_t dim){ return row*dim + col; }

DensityMatrix::DensityMatrix(std::size_t n) : n_(n), rho_( (std::size_t(1)<<n)*(std::size_t(1)<<n), {0.0,0.0} ){
  // |0..0><0..0|
  rho_[0] = {1.0,0.0};
}

void DensityMatrix::renormalize_(){
  // Ensure trace=1 (robustness)
  std::size_t d = dim();
  double tr=0.0;
  for (std::size_t i=0;i<d;i++) tr += std::real(rho_[idx(i,i,d)]);
  if (tr==0.0) return;
  double inv = 1.0/tr;
  for (auto& z : rho_) z *= inv;
}

void DensityMatrix::apply_unitary_1q(std::size_t target, const c64 u00, const c64 u01, const c64 u10, const c64 u11){
  const std::size_t d = dim();
  const std::size_t m = std::size_t(1) << target;
  // Apply U on rows and U* on cols: rho' = U rho U^\dagger
  // Row transform
  std::vector<c64> tmp(rho_.size());
  for (std::size_t r=0;r<d;++r){
    std::size_t r0 = (r & ~m);
    std::size_t r1 = r0 | m;
    // which pair? compute base rows
    if ((r & m)==0){
      for (std::size_t c=0;c<d;++c){
        tmp[idx(r,c,d)] = u00*rho_[idx(r,c,d)] + u01*rho_[idx(r1,c,d)];
      }
    } else {
      for (std::size_t c=0;c<d;++c){
        tmp[idx(r,c,d)] = u10*rho_[idx(r0,c,d)] + u11*rho_[idx(r,c,d)];
      }
    }
  }
  // Column transform by U^\dagger
  std::vector<c64> out(rho_.size());
  auto u00c = std::conj(u00), u01c = std::conj(u01), u10c = std::conj(u10), u11c = std::conj(u11);
  for (std::size_t c=0;c<d;++c){
    std::size_t c0 = (c & ~m);
    std::size_t c1 = c0 | m;
    if ((c & m)==0){
      for (std::size_t r=0;r<d;++r){
        out[idx(r,c,d)] = tmp[idx(r,c,d)]*u00c + tmp[idx(r,c1,d)]*u10c;
      }
    } else {
      for (std::size_t r=0;r<d;++r){
        out[idx(r,c,d)] = tmp[idx(r,c0,d)]*u01c + tmp[idx(r,c,d)]*u11c;
      }
    }
  }
  rho_.swap(out);
  renormalize_();
}

void DensityMatrix::apply_cx(std::size_t control, std::size_t target){
  // As unitary with permutation effect on basis states
  const std::size_t d = dim();
  const std::size_t cm = std::size_t(1) << control;
  const std::size_t tm = std::size_t(1) << target;
  std::vector<c64> out(rho_.size(), {0.0,0.0});
  for (std::size_t r=0;r<d;++r){
    std::size_t r2 = ( (r & cm) ? (r ^ tm) : r );
    for (std::size_t c=0;c<d;++c){
      std::size_t c2 = ( (c & cm) ? (c ^ tm) : c );
      out[idx(r2,c2,d)] = rho_[idx(r,c,d)];
    }
  }
  rho_.swap(out);
}

void DensityMatrix::dephase(std::size_t target, double p){
  // Kraus: sqrt(1-p) I, sqrt(p) Z
  using namespace qsx::gates;
  c64 u00,u01,u10,u11;
  Z_coeffs(u00,u01,u10,u11);
  // E[rho] = (1-p) rho + p Z rho Z
  std::vector<c64> tmp = rho_;
  // Z rho Z: flip phase on |1> rows and cols
  const std::size_t d = dim();
  const std::size_t m = std::size_t(1) << target;
  for (std::size_t r=0;r<d;++r){
    bool rz = (r & m);
    for (std::size_t c=0;c<d;++c){
      bool cz = (c & m);
      c64 val = rho_[idx(r,c,d)];
      if (rz ^ cz) val = -val;
      tmp[idx(r,c,d)] = val;
    }
  }
  for (std::size_t i=0;i<rho_.size();++i){
    rho_[i] = (1.0 - p)*rho_[i] + p*tmp[i];
  }
  renormalize_();
}

void DensityMatrix::depolarize(std::size_t target, double p){
  // E[rho] = (1-p)rho + p/3 (X rho X + Y rho Y + Z rho Z)
  std::vector<c64> acc(rho_.size(), {0.0,0.0});
  auto apply_pauli = [&](int which){
    std::vector<c64> out(rho_.size(), {0.0,0.0});
    const std::size_t d = dim();
    const std::size_t m = std::size_t(1) << target;
    if (which==0){ // X flips target bit on rows/cols
      for (std::size_t r=0;r<d;++r){
        std::size_t r2 = r ^ m;
        for (std::size_t c=0;c<d;++c){
          std::size_t c2 = c ^ m;
          out[idx(r2,c2,d)] = rho_[idx(r,c,d)];
        }
      }
    } else if (which==1){ // Y: like X with i/-i phases
      for (std::size_t r=0;r<d;++r){
        std::size_t r2 = r ^ m;
        bool rb = r & m;
        for (std::size_t c=0;c<d;++c){
          std::size_t c2 = c ^ m;
          bool cb = c & m;
          c64 v = rho_[idx(r,c,d)];
          if (rb) v *= c64{-1,0};
          else v *= c64{0,1};
          if (cb) v *= c64{0,1};
          else v *= c64{-1,0};
          out[idx(r2,c2,d)] = v;
        }
      }
    } else { // Z: sign on |1> rows/cols
      const std::size_t d2 = d;
      for (std::size_t r=0;r<d2;++r){
        bool rb = r & m;
        for (std::size_t c=0;c<d2;++c){
          bool cb = c & m;
          c64 v = rho_[idx(r,c,d2)];
          if (rb ^ cb) v = -v;
          out[idx(r,c,d2)] = v;
        }
      }
    }
    for (std::size_t i=0;i<acc.size();++i) acc[i] += out[i];
  };
  apply_pauli(0); apply_pauli(1); apply_pauli(2);
  for (std::size_t i=0;i<rho_.size();++i){
    rho_[i] = (1.0 - p)*rho_[i] + (p/3.0)*acc[i];
  }
  renormalize_();
}

DMRunResult run_density(const Circuit& c, uint64_t seed, bool collapse){
  (void)collapse; // density matrix keeps mixed states; collapse not applied
  DensityMatrix dm(c.nqubits);
  using namespace qsx::gates;
  c64 u00,u01,u10,u11;
  for (const auto& op : c.ops){
    switch(op.type){
      case OpType::H: H_coeffs(u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
      case OpType::X: X_coeffs(u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
      case OpType::Y: Y_coeffs(u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
      case OpType::Z:
      Z_coeffs(u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
    case OpType::S:
      S_coeffs(u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
    case OpType::RX:
      RX_coeffs(op.angle,u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
    case OpType::RY:
      RY_coeffs(op.angle,u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
    case OpType::Z: Z_coeffs(u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
      case OpType::RZ: RZ_coeffs(op.angle,u00,u01,u10,u11); dm.apply_unitary_1q(op.qubits[0],u00,u01,u10,u11); break;
      case OpType::CNOT: dm.apply_cx(op.qubits[0], op.qubits[1]); break;
      case OpType::DEPHASE: dm.dephase(op.qubits[0], op.angle); break;
      case OpType::DEPOL: dm.depolarize(op.qubits[0], op.angle); break;
      case OpType::AMPDAMP: dm.amp_damp(op.qubits[0], op.angle); break;
      case OpType::MEASURE: break;
    }
  }
  // Probabilities = diag(rho)
  DMRunResult rr;
  std::size_t d = dm.dim();
  rr.probabilities.resize(d);
  for (std::size_t i=0;i<d;++i) rr.probabilities[i] = std::real(dm.data()[idx(i,i,d)]);
  // Sample one outcome deterministically from seed
  Rng rng(seed);
  double r = rng.uniform();
  double acc=0.0; std::size_t idxv=0;
  for (std::size_t i=0;i<d;++i){ acc += rr.probabilities[i]; if (r<=acc){ idxv=i; break; } }
  rr.outcome.resize(c.nqubits);
  for (std::size_t q=0;q<c.nqubits;++q) rr.outcome[q] = (idxv>>q)&1;
  return rr;
}

} // namespace qsx

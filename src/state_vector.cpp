// SPDX-License-Identifier: MIT

#include "quantum/state_vector.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#ifdef QSX_OPENMP
#include <omp.h>
#endif

namespace qsx {

StateVector::StateVector(std::size_t n) : n_(n), amp_(std::size_t(1) << n, c64{0.0, 0.0}) {
  amp_[0] = {1.0, 0.0};
}

void StateVector::normalize_() {
  double norm2 = 0.0, c=0.0; for (auto& a : amp_) { double y = std::norm(a) - c; double t = norm2 + y; c = (t - norm2) - y; norm2 = t; }
  double inv = 1.0 / std::sqrt(norm2);
  for (auto& a : amp_) a *= inv;
}

void StateVector::apply_gate_1q(std::size_t target, const c64 u00, const c64 u01, const c64 u10, const c64 u11) {
  const std::size_t N = amp_.size();
  const std::size_t mask = std::size_t(1) << target;
  #ifdef QSX_OPENMP
#pragma omp parallel for schedule(static)
#endif
#ifdef QSX_OPENMP
#pragma omp parallel for schedule(static)
#endif
for (std::size_t i = 0; i < N; ++i) {
    if ((i & mask) == 0) {
      const std::size_t j = i | mask;
      c64 a0 = amp_[i];
      c64 a1 = amp_[j];
      amp_[i] = u00 * a0 + u01 * a1;
      amp_[j] = u10 * a0 + u11 * a1;
    }
  }
  if ((++applied_ & 255) == 0) normalize_();
}

void StateVector::apply_cx(std::size_t control, std::size_t target) {
  if (control == target) return;
  const std::size_t N = amp_.size();
  const std::size_t cm = std::size_t(1) << control;
  const std::size_t tm = std::size_t(1) << target;
  #ifdef QSX_OPENMP
#pragma omp parallel for schedule(static)
#endif
for (std::size_t i = 0; i < N; ++i) {
    if ((i & cm) && !(i & tm)) {
      std::size_t j = i | tm;
      std::swap(amp_[i], amp_[j]);
    }
  }
}

void StateVector::apply_controlled_1q(std::size_t control, std::size_t target, const c64 u00, const c64 u01, const c64 u10, const c64 u11) {
  if (control == target) return;
  const std::size_t N = amp_.size();
  const std::size_t cm = std::size_t(1) << control;
  const std::size_t tm = std::size_t(1) << target;
  #ifdef QSX_OPENMP
#pragma omp parallel for schedule(static)
#endif
for (std::size_t i = 0; i < N; ++i) {
    if ((i & cm) && !(i & tm)) {
      const std::size_t j = i | tm;
      c64 a0 = amp_[i];
      c64 a1 = amp_[j];
      amp_[i] = u00 * a0 + u01 * a1;
      amp_[j] = u10 * a0 + u11 * a1;
    }
  }
  if ((++applied_ & 255) == 0) normalize_();
}

double StateVector::probability_of_basis(std::size_t basis_index) const {
  return std::norm(amp_.at(basis_index));
}

std::vector<int> StateVector::measure_all(Rng& rng, bool collapse) {
  const std::size_t N = amp_.size();
  // Cumulative distribution
  double r = rng.uniform();
  double acc = 0.0;
  std::size_t idx = 0;
  #ifdef QSX_OPENMP
#pragma omp parallel for schedule(static)
#endif
for (std::size_t i = 0; i < N; ++i) {
    acc += std::norm(amp_[i]);
    if (r <= acc) { idx = i; break; }
  }
  std::vector<int> bits(n_, 0);
  for (std::size_t q = 0; q < n_; ++q) bits[q] = (idx >> q) & 1;
  if (collapse) {
    for (std::size_t i = 0; i < N; ++i) amp_[i] = {0.0, 0.0};
    amp_[idx] = {1.0, 0.0};
  }
  return bits;
}

} // namespace qsx


#include <fstream>
#include <optional>

namespace qsx {
bool StateVector::save(const std::string& path) const {
  struct Header{ char magic[8]; uint32_t version; uint32_t flags; uint64_t n; } h; std::memcpy(h.magic, "QSXSNP1", 8); h.version=1; h.flags=0; h.n=n_;
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(&h), sizeof(h));
  out.write(reinterpret_cast<const char*>(amp_.data()), sizeof(c64)*amp_.size());
  return bool(out);
}

std::optional<StateVector> StateVector::load(const std::string& path, std::size_t n_expected){
  struct Header{ char magic[8]; uint32_t version; uint32_t flags; uint64_t n; } h;
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  in.read(reinterpret_cast<char*>(&h), sizeof(h)); if (!in) return std::nullopt; if (std::string(h.magic, h.magic+7) != std::string("QSXSNP1",7)) return std::nullopt; if (h.version!=1) return std::nullopt; if (n_expected && h.n!=n_expected) return std::nullopt; StateVector sv((std::size_t)h.n);
  in.read(reinterpret_cast<char*>(sv.amp_.data()), sizeof(c64)*sv.amp_.size());
  if (!in) return std::nullopt;
  sv.normalize_();
  return sv;
}
} // namespace qsx

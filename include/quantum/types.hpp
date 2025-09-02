// SPDX-License-Identifier: MIT

#pragma once
#include <complex>
#include <vector>
#include <string>
#include <cstdint>

namespace qsx {
#ifdef QSX_FP32
  using c64 = std::complex<float>;
#else
  using c64 = std::complex<double>;
#endif
  using vec_c64 = std::vector<c64>;
}

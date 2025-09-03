// SPDX-License-Identifier: MIT

#pragma once
#include "types.hpp"
#include <numbers>

namespace qsx::gates {
  inline void X_coeffs(qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11) {
    u00 = {0,0}; u01 = {1,0}; u10 = {1,0}; u11 = {0,0};
  }
  inline void H_coeffs(qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11) {
    double s = 1.0/std::sqrt(2.0);
    u00 = {s,0}; u01 = {s,0}; u10 = {s,0}; u11 = {-s,0};
  }
  inline void Z_coeffs(qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11) {
    u00 = {1,0}; u01 = {0,0}; u10 = {0,0}; u11 = {-1,0};
  }
  inline void RZ_coeffs(double theta, qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11) {
    // diag(e^{-iθ/2}, e^{iθ/2})
    double half = theta/2.0;
    u00 = { std::cos(-half), std::sin(-half) };
    u11 = { std::cos( half), std::sin( half) };
    u01 = {0,0}; u10 = {0,0};
  }
}


inline void Y_coeffs(qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11) {
  // [[0, -i],[i,0]]
  u00 = {0,0}; u01 = {0,-1}; u10 = {0,1}; u11 = {0,0};
}


inline void RX_coeffs(double theta, qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11){
  double c = std::cos(theta/2.0);
  double s = std::sin(theta/2.0);
  u00 = {c,0}; u01 = {0,-s}; u10 = {0,-s}; u11 = {c,0};
}
inline void RY_coeffs(double theta, qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11){
  double c = std::cos(theta/2.0);
  double s = std::sin(theta/2.0);
  u00 = {c,0}; u01 = {s,0}; u10 = {-s,0}; u11 = {c,0};
}
inline void S_coeffs(qsx::c64& u00, qsx::c64& u01, qsx::c64& u10, qsx::c64& u11){
  u00 = {1,0}; u01 = {0,0}; u10 = {0,0}; u11 = {0,1}; // diag(1, i)
}

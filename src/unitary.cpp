// SPDX-License-Identifier: MIT

#include "quantum/unitary.hpp"
#include "quantum/gates.hpp"
#include <cmath>
#include <fstream>

namespace qsx {

static std::vector<c64> eye(std::size_t d){
  std::vector<c64> m(d*d, {0.0,0.0});
  for (std::size_t i=0;i<d;i++) m[i*d+i] = {1.0,0.0};
  return m;
}

static std::vector<c64> kron2(const std::vector<c64>& A, std::size_t arows, std::size_t acols,
                              const std::vector<c64>& B, std::size_t brows, std::size_t bcols){
  std::vector<c64> K(arows*brows*acols*bcols);
  for (std::size_t i=0;i<arows;i++)
    for (std::size_t j=0;j<acols;j++)
      for (std::size_t r=0;r<brows;r++)
        for (std::size_t s=0;s<bcols;s++)
          K[(i*brows+r)*(acols*bcols) + (j*bcols+s)] = A[i*acols+j]*B[r*bcols+s];
  return K;
}

static std::vector<c64> matmul(const std::vector<c64>& A, const std::vector<c64>& B, std::size_t d){
  std::vector<c64> C(d*d, {0.0,0.0});
  for (std::size_t i=0;i<d;i++)
    for (std::size_t k=0;k<d;k++){
      auto aik = A[i*d+k];
      for (std::size_t j=0;j<d;j++)
        C[i*d+j] += aik * B[k*d+j];
    }
  return C;
}

static std::vector<c64> gate_1q_matrix(const c64 u00,const c64 u01,const c64 u10,const c64 u11, std::size_t n, std::size_t target){
  // Build U (2^n) as I⊗...⊗U⊗...⊗I with target at position
  std::vector<c64> U2 = {u00,u01,u10,u11}; // row-major 2x2
  std::vector<c64> M = U2;
  std::size_t left = target, right = n - target - 1;
  for (std::size_t i=0;i<left;i++)  M = kron2({{1,0},{0,0},{0,0},{1,0}},2,2, M, 2, M.size()/2); // I ⊗ M
  for (std::size_t i=0;i<right;i++) M = kron2(M, M.size()/2, 2, {{1,0},{0,0},{0,0},{1,0}}, 2, 2); // M ⊗ I
  return M;
}

std::vector<c64> build_unitary(const Circuit& c){
  // Validate
  for (auto& op: c.ops){
    if (op.type==OpType::MEASURE || op.type==OpType::DEPHASE || op.type==OpType::DEPOL || op.type==OpType::AMPDAMP)
      throw std::runtime_error("Non-unitary op present");
  }
  std::size_t n = c.nqubits;
  std::size_t d = std::size_t(1) << n;
  auto U = eye(d);
  using namespace qsx::gates; c64 u00,u01,u10,u11;
  for (auto& op : c.ops){
    if (op.qubits.size()==1){
      switch(op.type){
        case OpType::H: H_coeffs(u00,u01,u10,u11); break;
        case OpType::X: X_coeffs(u00,u01,u10,u11); break;
        case OpType::Y: Y_coeffs(u00,u01,u10,u11); break;
        case OpType::Z: Z_coeffs(u00,u01,u10,u11); break;
        case OpType::S: S_coeffs(u00,u01,u10,u11); break;
        case OpType::RX: RX_coeffs(op.angle,u00,u01,u10,u11); break;
        case OpType::RY: RY_coeffs(op.angle,u00,u01,u10,u11); break;
        case OpType::RZ: RZ_coeffs(op.angle,u00,u01,u10,u11); break;
        default: throw std::runtime_error("Unsupported unitary op");
      }
      auto G = gate_1q_matrix(u00,u01,u10,u11,n, op.qubits[0]);
      U = matmul(G, U, d);
    } else if (op.type==OpType::CNOT){
      // Build CNOT on (control,target)
      std::size_t cbit = op.qubits[0], tbit = op.qubits[1];
      // Apply as permutation on the basis states: we can compose into U by permuting columns
      // Construct P such that |i> -> |i ^ ( (i>>c)&1 ? (1<<t) : 0 )>
      std::vector<c64> P(d*d, {0.0,0.0});
      for (std::size_t i=0;i<d;i++){
        bool c1 = ( (i>>cbit) & 1 ) != 0;
        std::size_t j = c1 ? (i ^ (std::size_t(1)<<tbit)) : i;
        P[j*d + i] = {1.0,0.0};
      }
      U = matmul(P, U, d);
    } else {
      throw std::runtime_error("Unsupported multi-qubit op");
    }
  }
  return U;
}

bool export_unitary_csv(const Circuit& c, const std::string& path){
  std::size_t d = std::size_t(1) << c.nqubits;
  if (d > (1u<<10)) return false; // safety: limit to 10 qubits
  auto U = build_unitary(c);
  std::ofstream out(path);
  if (!out) return false;
  for (std::size_t i=0;i<d;i++){
    for (std::size_t j=0;j<d;j++){
      auto z = U[i*d+j];
      out << std::real(z) << "+" << std::imag(z) << "i";
      if (j+1<d) out << ",";
    }
    out << "\n";
  }
  return bool(out);
}

} // namespace qsx

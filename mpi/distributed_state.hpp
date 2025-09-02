// SPDX-License-Identifier: MIT

#pragma once
#include "quantum/state_vector.hpp"
#ifdef QSX_MPI
#include <mpi.h>
#endif
#include <optional>

namespace qsx {

struct MPIContext {
  int rank=0, size=1;
  std::size_t nqubits=0, local_bits=0;
  std::size_t local_size=0; // 2^(n - log2(size))
};

std::optional<MPIContext> init_mpi_state(std::size_t n);
void finalize_mpi();

// Apply 1-qubit gate in distributed memory (partition by high bits).
void apply_gate_1q_mpi(StateVector& local, const MPIContext& ctx, std::size_t target,
                        const c64 u00, const c64 u01, const c64 u10, const c64 u11);

// Experimental CNOT across ranks (two-phase exchange)
void apply_cx_mpi(StateVector& local, const MPIContext& ctx, std::size_t control, std::size_t target);

} // namespace qsx

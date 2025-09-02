// SPDX-License-Identifier: MIT

#include "mpi/distributed_state.hpp"
#include <vector>
#include <algorithm>
#include <cstring>

namespace qsx {

std::optional<MPIContext> init_mpi_state(std::size_t n){
#ifndef QSX_MPI
  (void)n; return std::nullopt;
#else
  int inited=0; MPI_Initialized(&inited);
  if (!inited){ int argc=0; char** argv=nullptr; MPI_Init(&argc,&argv); }
  int rank=0,size=1; MPI_Comm_rank(MPI_COMM_WORLD,&rank); MPI_Comm_size(MPI_COMM_WORLD,&size);
  // size must be power of two and <= 2^n
  int s = size; bool pow2 = (s && !(s & (s-1)));
  if (!pow2) return std::nullopt;
  int bits=0; while ((1<<bits)<size) ++bits;
  MPIContext ctx; ctx.rank=rank; ctx.size=size; ctx.nqubits=n; ctx.local_bits=bits;
  ctx.local_size = std::size_t(1) << (n - bits);
  return ctx;
#endif
}

void finalize_mpi(){
#ifdef QSX_MPI
  int finalized=0; MPI_Finalized(&finalized);
  if (!finalized) MPI_Finalize();
#endif
}

static inline std::size_t partner_rank(const MPIContext& ctx, std::size_t global_bit){
  std::size_t rel = global_bit - ctx.local_bits;
  return ctx.rank ^ (1u << rel);
}

void apply_gate_1q_mpi(StateVector& local, const MPIContext& ctx, std::size_t target,
                        const c64 u00, const c64 u01, const c64 u10, const c64 u11){
#ifndef QSX_MPI
  (void)local; (void)ctx; (void)target; (void)u00;(void)u01;(void)u10;(void)u11;
#else
  const std::size_t Nloc = local.dimension();
  const bool is_local_bit = target < (ctx.nqubits - ctx.local_bits);
  if (is_local_bit){
    // Local apply identical to single-process
    local.apply_gate_1q(target, u00,u01,u10,u11);
  } else {
    // Need partner exchange for the high bit
    int peer = (int)partner_rank(ctx, target);
    std::vector<c64> recv(Nloc);
    MPI_Sendrecv(local.amplitudes().data(), (int)Nloc*sizeof(c64), MPI_BYTE, peer, 0,
                 recv.data(), (int)Nloc*sizeof(c64), MPI_BYTE, peer, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    // Combine with partner amplitudes
    // Determine if our rank has bit=0 or 1 for the target
    bool my_bit_one = ((ctx.rank >> (target - ctx.local_bits)) & 1) != 0;
    // We need to form pairs (a0 from rank with bit=0, a1 from rank with bit=1)
    // If my_bit_one==0, local is a0 and recv is a1; else reversed.
    auto& a = const_cast<qsx::vec_c64&>(local.amplitudes());
    if (!my_bit_one){
      for (std::size_t i=0;i<Nloc;++i){
        c64 a0 = a[i], a1 = recv[i];
        a[i] = u00*a0 + u01*a1;
        recv[i] = u10*a0 + u11*a1;
      }
    } else {
      for (std::size_t i=0;i<Nloc;++i){
        c64 a1 = a[i], a0 = recv[i];
        a[i] = u10*a0 + u11*a1;
        recv[i] = u00*a0 + u01*a1;
      }
    }
    // Send back the partner's updated half
    MPI_Sendrecv(recv.data(), (int)Nloc*sizeof(c64), MPI_BYTE, peer, 1,
                 nullptr, 0, MPI_BYTE, peer, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    // Note: partner performs the symmetric update; local state already updated
  }
#endif
}

void apply_cx_mpi(StateVector& local, const MPIContext& ctx, std::size_t control, std::size_t target){
#ifndef QSX_MPI
  (void)local; (void)ctx; (void)control; (void)target;
#else
  // Simple approach: if target is local bit, we can swap pairs locally for indexes with control=1
  const std::size_t Nloc = local.dimension();
  const bool t_local = target < (ctx.nqubits - ctx.local_bits);
  const bool c_local = control < (ctx.nqubits - ctx.local_bits);
  auto& a = const_cast<qsx::vec_c64&>(local.amplitudes());
  if (t_local && c_local){
    const std::size_t tm = std::size_t(1) << target;
    const std::size_t cm = std::size_t(1) << control;
    for (std::size_t i=0;i<Nloc;++i){
      if ((i & cm) && !(i & tm)){
        std::size_t j = i | tm;
        std::swap(a[i], a[j]);
      }
    }
    return;
  }
  // Exact rank-partitioned CNOT using global index mapping.
  auto& loc = a; // alias
  // helper lambdas
  auto global_bit = [&](std::size_t local_index, std::size_t bit)->int{
    std::size_t low_bits = ctx.nqubits - ctx.local_bits;
    if (bit < low_bits) return ( (local_index >> bit) & 1 );
    int rbit = int(bit - low_bits);
    return ( (ctx.rank >> rbit) & 1 );
  };
  auto partner_for_bit = [&](std::size_t bit)->int{
    int rbit = int(bit - (ctx.nqubits - ctx.local_bits));
    return int(ctx.rank ^ (1<<rbit));
  };
  bool t_high = target >= (ctx.nqubits - ctx.local_bits);
  int peer = t_high ? partner_for_bit(target) : ctx.rank;
  std::vector<c64> peerbuf(Nloc);
  if (t_high){
    MPI_Sendrecv(loc.data(), (int)Nloc*sizeof(c64), MPI_BYTE, peer, 20,
                 peerbuf.data(), (int)Nloc*sizeof(c64), MPI_BYTE, peer, 20,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
  std::vector<c64> out_local = loc;
  std::vector<c64> out_peer  = peerbuf;
  bool my_tbit1 = t_high ? ((ctx.rank >> (target - (ctx.nqubits - ctx.local_bits))) & 1) : false;
  for (std::size_t i=0;i<Nloc;++i){
    int cval = global_bit(i, control);
    if (cval==1){
      if (!t_high){
        std::size_t j = i ^ (std::size_t(1) << target);
        if ((i & (std::size_t(1)<<target))==0){
          std::swap(out_local[i], out_local[j]);
        }
      } else {
        // target is high bit => swap across ranks if my target bit is 0
        if (!my_tbit1){
          // partner index same i (low bits unchanged)
          std::swap(out_local[i], out_peer[i]);
        }
      }
    }
  }
  // Commit updates
  loc.swap(out_local);
  if (t_high){
    MPI_Sendrecv(out_peer.data(), (int)Nloc*sizeof(c64), MPI_BYTE, peer, 21,
                 nullptr, 0, MPI_BYTE, peer, 21, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
#endif
}

} // namespace qsx

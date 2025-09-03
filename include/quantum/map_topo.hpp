
// SPDX-License-Identifier: MIT
#pragma once
#include "circuit.hpp"
#include <vector>
#include <queue>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace qsx {

// Reads an undirected topology graph from file: each line "u v" (0-based qubit indices)
inline std::vector<std::vector<std::size_t>> read_topology(const std::string& path, std::size_t nqubits){
  std::vector<std::vector<std::size_t>> adj(nqubits);
  std::ifstream in(path);
  std::size_t u,v;
  while (in >> u >> v){
    if (u<nqubits && v<nqubits){
      adj[u].push_back(v);
      adj[v].push_back(u);
    }
  }
  return adj;
}

inline std::vector<std::size_t> shortest_path(const std::vector<std::vector<std::size_t>>& adj, std::size_t s, std::size_t t){
  if (s==t) return {s};
  std::vector<int> prev(adj.size(), -1);
  std::queue<std::size_t> q; q.push(s); prev[s] = (int)s;
  while (!q.empty()){
    auto x = q.front(); q.pop();
    for (auto y : adj[x]){
      if (prev[y]==-1){
        prev[y] = (int)x; q.push(y);
        if (y==t) break;
      }
    }
  }
  if (prev[t]==-1) return {}; // disconnected
  std::vector<std::size_t> path; path.push_back(t);
  while (path.back()!=s){ path.push_back((std::size_t)prev[path.back()]); }
  std::reverse(path.begin(), path.end());
  return path;
}

// Map circuit to arbitrary topology by inserting SWAPs along shortest paths for CNOT
inline Circuit map_to_topology(const Circuit& in, const std::vector<std::vector<std::size_t>>& adj){
  Circuit out; out.nqubits = in.nqubits;
  std::vector<std::size_t> phys(in.nqubits); for (std::size_t i=0;i<in.nqubits;++i) phys[i]=i;
  auto emit_swap = [&](std::size_t a, std::size_t b){
    out.ops.push_back({OpType::CNOT,{a,b},0.0});
    out.ops.push_back({OpType::CNOT,{b,a},0.0});
    out.ops.push_back({OpType::CNOT,{a,b},0.0});
  };
  auto swap_positions = [&](std::size_t a, std::size_t b){
    for (std::size_t l=0;l<phys.size();++l){
      if (phys[l]==a) phys[l]=b;
      else if (phys[l]==b) phys[l]=a;
    }
  };
  for (const auto& op : in.ops){
    if (op.type==OpType::CNOT && op.qubits.size()==2){
      std::size_t lc = op.qubits[0], lt = op.qubits[1];
      std::size_t pc = phys[lc], pt = phys[lt];
      // find path from pc to pt
      auto path = shortest_path(adj, pc, pt);
      if (path.size()<2){ out.ops.push_back({OpType::CNOT,{pc,pt},0.0}); continue; }
      // move target towards control along path via SWAPs
      for (std::size_t i=0;i+1<path.size()-1; ++i){
        emit_swap(path[i+1], path[i+2]); // swap next step
        swap_positions(path[i+1], path[i+2]);
      }
      // now adjacent
      pc = phys[lc]; pt = phys[lt];
      out.ops.push_back({OpType::CNOT,{pc,pt},0.0});
    } else if (op.qubits.size()==1){
      out.ops.push_back({op.type, { phys[op.qubits[0]] }, op.angle});
    } else {
      out.ops.push_back(op);
    }
  }
  return out;
}

} // namespace qsx

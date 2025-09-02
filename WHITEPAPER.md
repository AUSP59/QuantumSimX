
# QUANTUM-SIMX Whitepaper
**Version:** 1.2.0

## Overview
This document describes the algorithms and engineering behind QUANTUM-SIMX, a portable quantum circuit simulator with state-vector and density-matrix backends, a reproducible CLI, and a robust OSS governance model.

## Backends
- **State-vector (SV):** O(2^n) memory, fast Clifford+R rotations, MPI split and CX across partitions.
- **Density-matrix (DM):** O(4^n) memory, exact Markovian noise (dephasing, depolarizing, amplitude damping).

## Optimizer
Gate fusion and cancellation rules (H/H, X/X, Z/Z, S/S→Z), rotation merging, and CNOT-pair elimination.

## Noise & Mitigation
Readout error model (asymmetric) + mitigation via inverse confusion (Kronecker). Zero-Noise Extrapolation (ZNE) for ⟨Z_q⟩.

## Observables
⟨X⟩, ⟨Y⟩, ⟨Z⟩, ⟨Z_iZ_j⟩, Hamming weight, parity. Pauli-string expectations and unitary export (small-n).

## Reproducibility & Supply Chain
Snapshot format with versioned header, SBOM (CycloneDX), CI coverage gates, CodeQL, fuzzing, Scorecard, Dependabot, provenance skeleton.

## Complexity
- SV step: O(G 2^n) FLOPs; DM step: O(G 4^n).
- Memory guards and estimates enforced in CLI.

## Limitations
- Practical DM limited to ~10 qubits on commodity machines.
- For larger systems and acceleration use GPU/SIMD/MPI HPC (out-of-scope for base build).

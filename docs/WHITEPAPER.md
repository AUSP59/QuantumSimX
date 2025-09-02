
# QUANTUM-SIMX Whitepaper

## Problem
Accurate, reproducible multi-qubit simulation is compute- and memory-intensive. Many projects either overscope UI or underdeliver core physics.

## Approach
- State-vector simulation with cache-friendly stride loops.
- Branchless inner kernels where possible.
- Optional parallelism (OpenMP) and MPI partitioning (outline).

## Complexity
- Memory: O(2^n)
- Single-qubit gate: O(2^n)
- Controlled gate: O(2^n)

## Limits
- Practically ~26–30 qubits on laptops; >30 requires significant RAM/cluster.

## Reproducibility
- All measurements are seeded via Mersenne Twister.
- Deterministic ordering and normalization after operations.

## Benchmarks
Include micro-benchmarks using steady_clock; disclose hardware in reports.

## Ethics
No exaggerated claims; this is a classical simulator.

## Noise (Optional)
- Dephasing via random Pauli-Z with probability p.
- Depolarizing via random Pauli (X/Y/Z) with probability p.

## Parallelism
- Optional OpenMP parallel loops for gate application.

# QuantumSimX

World-class, production-ready **C++** quantum circuit simulator with **state-vector** & **density-matrix** backends, realistic noise + mitigation, topology mapping, rigorous provenance, and a rich CLI & C API. Portable, auditable, and reproducible.

<p align="center">
  <b>Status:</b> Production-ready • Deterministic RNG (PCG32) • Reproducible builds • Strong supply-chain hygiene
</p>

---

## Table of Contents
- [Features](#features)
- [Quick Start](#quick-start)
- [Build & Install](#build--install)
- [CLI Overview](#cli-overview)
- [C API](#c-api)
- [Reproducibility](#reproducibility)
- [Noise & Mitigation](#noise--mitigation)
- [Topology Mapping](#topology-mapping)
- [Metrics & Observability](#metrics--observability)
- [Examples](#examples)
- [Testing & Quality](#testing--quality)
- [Contributing & Conduct](#contributing--conduct)
- [Security](#security)
- [License](#license)
- [FAQ](#faq)

---

## Features
- **Backends**: State-vector (fast) & Density-matrix (noisy channels; warns for large `n`).
- **Noise**: Dephasing/Depolarizing/Amplitude-damping; readout model; **ZNE**; simple error maps.
- **Validation**: **Quantum Volume**, **RB (1Q)**, **Bell-CHSH**, tomography **1Q/2Q**, stabilizer detection.
- **Analysis**: Entropy, fidelity, mutual info, Wilson/Bootstrap intervals, resource/memory estimates.
- **Mapping & Scheduling**: Line/graph topologies, makespan estimation, safe commuting optimizer.
- **Provenance**: Version, input hashes, `runId`, optional `runTimestamp`, `topologyHashFNV1a`, `z_expect`, `zz_matrix`, `zz_cov`.
- **Interoperability**: QASM/QSX I/O, `state-npy`, counts→CSV, NDJSON streaming, Prometheus metrics.
- **APIs**: Powerful CLI; minimal C API (`qsx_run_string`, `qsx_run_file`, `qsx_version`) and `pkg-config`.
- **DX & Ops**: Manpages, Doxygen, CPack, Docker/DevContainer, SBOM, CodeQL, fuzzing, OpenSSF hygiene.

---

## Quick Start
```bash
# Clone
git clone https://github.com/AUSP59/QuantumSimX.git
cd QuantumSimX

# Configure & Build (C++17+)
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# Smoke test
./build/bin/quantum-simx --help
Run a Bell circuit:

bash
Copiar código
./build/bin/quantum-simx run --qasm examples/bell.qasm --shots 4096 --seed 42 --out out.json
jq '.provenance, .counts, .shannon_bits' out.json
Build & Install
Portable CMake
bash
Copiar código
# Release build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /usr/local
Windows (MSVC)
powershell
Copiar código
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -- /m
Package (CPack)
bash
Copiar código
cpack --config build/CPackConfig.cmake
pkg-config (C++ core & C API)
bash
Copiar código
pkg-config --cflags --libs quantum-simx
CLI Overview
Use --help on any subcommand. Manpage: man quantum-simx.

Core execution

run / mrun / stream — simulate QASM/QSX; NDJSON streaming; --density for DM.

export-qasm, state, counts-csv, report, stats.

Analysis & validation

entropy, fidelity, mutual, compare, intervals, bootstrap, shots-plan.

resources (gate counts/depth/memory), schedule (makespan), cost (makespan + success prob).

stabilizer (Clifford check), chsh (Bell), verify (Bell/GHZ/QFT CI sanity).

Tomography

tomo1q-plan / tomo1q-aggregate

tomo2q-plan / tomo2q-aggregate

Optimization & mapping

opt-commute (safe Z/RZ through CNOT; merge Z-family), canonicalize (stable diffs).

map-topology, map-line (via flags in run).

Observability

metrics (Prometheus/OpenMetrics from JSON).

state-npy (NumPy snapshot of final state).

Utilities

qv, rb1q, zne, lint, xcheck, selftest, doctor, version, batch.

Example snippets
bash
Copiar código
# CHSH violation on a Bell state
quantum-simx chsh --qasm examples/bell.qasm --shots 8192

# Wilson intervals for counts
quantum-simx intervals --json out.json --z 1.96

# Bootstrap CI for top-10 outcomes
quantum-simx bootstrap --json out.json --reps 2000 --topk 10 --alpha 0.05

# Resources & memory estimates
quantum-simx resources --qasm examples/ghz5.qasm

# Partial trace (reduced density matrix)
quantum-simx partial-rho --qasm examples/ghz5.qasm --subset 0,1 --out rho.json

# NumPy state export
quantum-simx state-npy --qasm examples/ghz5.qasm --out ghz5_state.npy
C API
Header: include/quantum/c_api.h
Linking: pkg-config --cflags --libs quantum-simx

c
Copiar código
#include "quantum/c_api.h"
#include <stdio.h>

int main(void){
  const char* qasm = "OPENQASM 2.0; qreg q[2]; h q[0]; cx q[0],q[1];";
  char* json = NULL;
  int rc = qsx_run_string(qasm, "{\"shots\":1024}", &json);
  if (rc==0) { puts(json); }
  qsx_free(json);
  printf("version=%s\n", qsx_version());
  return rc;
}
Reproducibility
Deterministic RNG: PCG32; set --seed.

Timestamps: set SOURCE_DATE_EPOCH to embed a fixed runTimestamp.

Provenance includes:

version, inputHashFNV1a, runId, optional runTimestamp

topologyHashFNV1a (if --map-topology file used)

Derived stats: z_expect, zz_matrix, zz_cov

Use canonicalize for format-stable QSX.

Minimal example:

bash
Copiar código
export SOURCE_DATE_EPOCH=1700000000
quantum-simx run --qasm examples/bell.qasm --seed 7 --out out.json
jq '.provenance' out.json
Noise & Mitigation
Channels: DEPHASE, DEPOL, AMPDAMP (per-gate or global).

Readout error model + mitigation hooks.

ZNE: zero-noise extrapolation workflow built-in.

Topology Mapping
Provide a topology file (edges per line) and map via:

bash
Copiar código
quantum-simx run --qasm circ.qasm --map-topology topologies/line_8.topo --out out.json
Supported samples: line_8.topo, ring_8.topo, heavy_hex_8.topo.

schedule estimates makespan (ns) with timing.cfg.

Metrics & Observability
Export Prometheus/OpenMetrics from a run JSON:

bash
Copiar código
quantum-simx metrics --json out.json --namespace qsx | curl --data-binary @- http://pushgateway/metrics/job/qsx
Examples
examples/bell.qasm — Bell state

examples/ghz*.qsx/qasm — GHZ generators

Tomography plans: tomo1q-*, tomo2q-*

Topologies: topologies/*.topo

Configs: timing.cfg, errors.cfg

Testing & Quality
CTests (unit/property/regression), verify smoke for CI.

Static analysis: clang-tidy; formatting: clang-format.

Fuzzing corpus & sanitizers in CI; coverage targets ≥95% for touched code.

doctor checks presence of key files (license, schema, tooling).

Run:

bash
Copiar código
ctest --test-dir build --output-on-failure
./build/bin/quantum-simx verify --shots 4096
Contributing & Conduct
We welcome issues and PRs—please read:

CONTRIBUTING (workflow, style, tests, ABI/JSON stability)

CODE_OF_CONDUCT

Security
Private security contact: AUSP59 alanursapu@gmail.com

See SECURITY.md for responsible disclosure. No telemetry; metrics are opt-in.

License
Code: Apache-2.0 OR MIT (SPDX: Apache-2.0 OR MIT)

Docs & non-code: CC BY 4.0 (SPDX: CC-BY-4.0)
See LICENSE for details.

FAQ
Q: How large can I go with density-matrix?
A: DM scales as 4^n. The CLI warns at n ≥ 11. Prefer state-vector or tailor noise usage.

Q: Are runs reproducible across OS/compilers?
A: Yes—use fixed --seed and (optionally) SOURCE_DATE_EPOCH. RNG and provenance are portable.

Q: Can I integrate with my stack?
A: Use the C API and pkg-config. CLI emits JSON/NDJSON and Prometheus metrics for pipelines.

Maintainer: AUSP59 • Contact: alanursapu@gmail.com
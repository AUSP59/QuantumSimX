
# Changelog

## 0.1.0 - 2025-09-02
- Initial public release: state-vector simulator, CLI, tests, benchmarks, docs, CI configs, SBOM, CPack packaging.

## 0.2.0 - 2025-09-02
- Density-matrix backend with dephasing/depolarizing noise.
- Experimental MPI distributed single-qubit gates across partitions.
- JSON schema, MkDocs Pages, CodeQL, version/build-info CLI.
- Additional tests and reproducible build flags.

## 0.3.0 - 2025-09-02
- Added amplitude damping (density backend).
- Added expZ observables to CLI output.
- Added snapshot-out for state-vector runs.
- Optional FP32 precision toggle and Kahan normalization.
- Threat model documentation.

## 0.4.0 - 2025-09-02
- Circuit optimizer (fusion/cancellation/rotation merge, CNOT pair cancel).
- Snapshot format with magic & version.
- Memory guard and run timings in CLI.
- Optional expX/expY, and FP32 option documented.

## 0.6.0 - 2025-09-02
- Unitary export subcommand and unitary builder (small-n).
- ZZ correlation matrix `expZZ` in JSON when `--observables all`.
- INI-style config file for CLI defaults.
- CMake package-config install for downstream `find_package`.
- Coverage gate raised to 85%. ADRs, PRESS and BRAND docs added.

## 0.8.0 - 2025-09-02
- DOT export, parameter sweep, bench harness, QAOA generator.
- Governance & release docs; CODEOWNERS.
- Coverage gate raised to 95%.

## 0.9.0 - 2025-09-02
- Readout mitigation via Kronecker inverse; `probabilities_mitigated` field.
- Line-topology mapping pass with SWAP insertion (`--map-line`).
- Basic output checker subcommand.
- Manpage and README updated. Tests for mapping and mitigation.

## 1.0.0 - 2025-09-02
- New subcommands: `mrun` (multi-threaded), `stats`, `export-qasm`, `report`.
- Governance docs: SUPPORT.md, MAINTAINERS.md, BACKWARD_COMPATIBILITY.md.
- Project maturity: version bumped to 1.0.0.

## 1.1.0 - 2025-09-02
- New CLI: lint, xcheck, zne (zero-noise extrapolation), selftest.
- Bash completion, .clang-format/.clang-tidy, .editorconfig, SPDX checker tool.
- Docs updated.

## 1.2.0 - 2025-09-02
- C API (`quantum_simx_c`) with `qsx_run_string`.
- `state` subcommand to export amplitudes (n<=16).
- Extra observables in run JSON: `expHW`, `parityZ`, and `gateHistogram`.
- Issue/PR templates, SLSA provenance skeleton, NOTICE, example configs, WHITEPAPER.

## 1.3.0 - 2025-09-02
- Provenance block in JSON (embedded version + FNV-1a circuit hash).
- NDJSON streaming mode (`stream` subcommand).
- pkg-config for C API, bash completion installed by CMake.
- SPDX headers sweep across source tree; added API_C.md; manpage updated.

## 1.4.0 - 2025-09-02
- `entropy` subcommand (Rényi-2 on arbitrary subset).
- `fidelity` subcommand for |<psi_A|psi_B>|^2.
- Generators extended: `--teleport`, `--bv N --mask`.
- `counts-csv` exporter; `--pretty` flag in run.
- Schema updated to include `provenance`.
- CITATION.cff added.

## 1.5.0 - 2025-09-02
- General topology mapper (`--map-topology`) with shortest-path SWAP routing.
- Quantum Volume (`qv`) generator/evaluator (heavy output fraction).
- 1-qubit randomized benchmarking (`rb1q`) producing CSV.
- Extra run metrics: `shannon_bits`, `gini`.

## 1.6.0 - 2025-09-02
- New subcommands: `compare` (TV/KL/JS/Hellinger/Cosine), `canonicalize` for .qsx.
- Provenance extended: `runId` and optional `runTimestamp` (from SOURCE_DATE_EPOCH).
- Example topologies under `topologies/` and reproducibility guide.
- Manpage/README updated.

## 1.7.0 - 2025-09-02
- Deterministic RNG (PCG32) for stochastic CLI utilities (qv/rb1q/selftest).
- Provenance extended with `topologyHashFNV1a` when `--map-topology` is used.
- Reproducibility guide updated.

## 1.8.0 - 2025-09-02
- New subcommands: `intervals` (Wilson CI), `schedule` (timing model), `opt-commute` (safe Z/RZ commuting & merging), `tomo1q-plan` / `tomo1q-aggregate`, `doctor`.
- C API: `qsx_run_file`.
- Completions for zsh/fish; timing.cfg default; docs/man updated; CODEOWNERS & SECURITY_CONTACTS.

## 1.9.0 - 2025-09-02
- New subcommands: `version`, `resources` (counts/depth/memory), `mutual` (classical MI), `stabilizer` (Clifford detection).
- README/man updated; example heavy-hex topology.

## 2.0.0 - 2025-09-02
- `partial-rho` (reduced density matrix export for subsets).
- `shots-plan` (estimate shots for desired Wilson CI width).
- `metrics` exporter (Prometheus/OpenMetrics text from run JSON).
- C API: `qsx_version()`; pkg-config for C++ core (`quantum-simx.pc`).

## 2.1.0 - 2025-09-02
- Added `chsh` (Bell test), `bootstrap` CIs, `batch` runner, and `verify` suite.
- Density-backend safety warning for large `n`.
- README/man updated.

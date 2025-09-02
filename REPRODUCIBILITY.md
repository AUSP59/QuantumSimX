
# Reproducibility Guide

- **Seeds**: All stochastic paths derive from a base seed; CLI commands accept `--seed`. Multi-shot runs use `seed + i` for shot `i`.
- **Provenance**: The `run` JSON embeds `version`, `inputHashFNV1a` and a `runId` derived from the circuit hash. If `SOURCE_DATE_EPOCH` is set, `runTimestamp` is included.
- **Deterministic outputs**: Given the same inputs (circuit/QASM, flags, seeds, version), outputs and counts are bitwise identical.
- **Builds**: Set `SOURCE_DATE_EPOCH` for reproducible timestamps in archives/docs.


## RNG determinism
All stochastic CLI tools now use a **PCG32** RNG (portable, reproducible across platforms). Given the same seed and inputs, sequences are identical.

## Topology provenance
When `--map-topology` is used, the provenance block embeds `topologyHashFNV1a`, a 64‑bit FNV‑1a over the topology file bytes.

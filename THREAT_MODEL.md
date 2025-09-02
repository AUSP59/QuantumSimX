
# Threat Model (QUANTUM-SIMX)

## Assets
- Source code, build pipeline, release artifacts, SBOM, documentation.

## Trust boundaries
- Build runners (CI), optional dependencies (Qt, MPI, pybind11), user-provided circuits.

## Risks & Mitigations
- **Malicious circuits**: parser hardened, fuzzed; no dynamic execution; bounded memory via warnings.
- **Supply chain**: SBOM, CodeQL, DCO, pinned actions; optional provenance workflow.
- **Secrets**: none required; avoid embedding credentials.
- **Binary tampering**: recommend signing releases (Sigstore).

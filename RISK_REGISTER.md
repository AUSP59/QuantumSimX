
# Risk Register
- Numerical drift: mitigated by Kahan normalization, schema outputs, seeds.
- Memory blow-ups: guarded by estimates and `--force` gate.
- Parser fuzzing gaps: libFuzzer target shipped; keep seeds/regressions.
- Supply chain: SBOM, CodeQL, Dependabot, OpenSSF Scorecard.

# CONTRIBUTING to QuantumSimX

> Thank you for helping build a world-class, production-ready quantum simulator.  
> By participating, you agree to follow our **[Code of Conduct](CODE_OF_CONDUCT.md)**.

---

## Quick Start (Dev)

```bash
# 1) Clone
git clone https://github.com/AUSP59/QuantumSimX.git
cd QuantumSimX

# 2) Configure (C++17+)
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3) Build
cmake --build build -j

# 4) Run tests (CTests wired to unit/property/fuzz smoke)
ctest --test-dir build --output-on-failure

# 5) Lint & format
./tools/format.sh && ./tools/lint.sh  # or run pre-commit (see below)

# 6) Local CLI
./build/bin/quantum-simx --help
Tip: reproducible runs → set --seed and (optional) SOURCE_DATE_EPOCH.
Our RNG is PCG32; provenance includes version, inputHashFNV1a, runId, optional runTimestamp, and topologyHashFNV1a.

How to Contribute
Bugs: open an Issue with a minimal repro (CLI cmd, QASM/QSX snippet, expected vs. actual, OS, compiler, quantum-simx version, seeds, topology file, provenance block).

Features: open a Discussion or “Feature request” Issue with a concise design sketch (goal, API/CLI, JSON schema impact, tests, docs, risks).

Docs: PRs welcome (README/manpages/whitepaper/tutorials). Keep examples runnable.

First PRs: feel free to start with docs, tests, or small fixes.

Development Standards
Language, Build, Targets
C++17 or later (portable across GCC/Clang/MSVC).

Build via CMake; tests via CTest.

Cross-platform: Linux, macOS, Windows. CI runs a matrix of compilers.

Style & Tooling
Formatting: .clang-format (run tools/format.sh).

Static analysis: .clang-tidy (run tools/lint.sh).

Editor settings: .editorconfig.

SPDX headers on all sources; no new files without license headers.

No naked new/delete; prefer RAII/unique_ptr.

APIs: prefer std::span, std::optional, string_view, noexcept where sane.

Thread-safety: per-thread RNG, no shared mutable state without guarding.

Tests & Quality Bars
Add/extend tests for every bugfix/feature.

Keep tests deterministic (set seeds). No flakiness.

Target ≥95% coverage for touched areas; justify exceptions.

Add property checks where possible (e.g., probabilities sum ≈1, CPTP constraints).

Large changes: include quick bench or complexity rationale.

Commit & PR Guidelines
Conventional Commits
Use:

makefile
Copiar código
feat(cli): add `compare` JS/Hellinger metrics
fix(dm): avoid overflow in Kraus apply
perf(sim): fuse 1Q gates in SV kernel
docs(man): document `tomo1q-aggregate`
test(qv): add QV regression
build(cmake): harden relro/now
ci: add sanitizer job
chore: refresh SBOM
PR Checklist
 Follows Conventional Commits.

 Code formatted & linted; no new warnings.

 Tests added/updated; ctest green.

 Docs updated (README, examples, docs/man/quantum-simx.1).

 JSON schema updated if outputs changed (docs/schema/*.json) + fixtures.

 Completions updated if CLI changed (extras/completion/*).

 CHANGELOG.md entry (Keep a Changelog).

 Provenance remains correct; determinism preserved with fixed seed.

 SBOM/regulatory metadata intact; SPDX headers present.

Small PRs are easier to review; large ones should be split or preceded by a short design note.

Extending the Project
Adding a CLI subcommand
Implement in cli/main.cpp (keep I/O stable and streaming-friendly).

Update:

README.md (usage & examples)

docs/man/quantum-simx.1 (flags, env, exit codes)

Shell completions: extras/completion/_quantum-simx, quantum-simx.fish

Tests (CLI golden outputs / property tests)

Modifying JSON outputs
Update docs/schema/output.schema.json and test fixtures.

Include provenance fields if you add new inputs affecting reproducibility.

Avoid breaking changes; if necessary, document migration.

C API / ABI
Keep include/quantum/c_api.h stable; document any new symbols.

Return error codes, never throw across C ABI. Document ownership (qsx_free).

Reproducibility & Scientific Rigor
Provide seeds (--seed), environment (SOURCE_DATE_EPOCH), and if relevant topology files for mapping; the provenance block must reproduce your results.

Prefer Wilson intervals for counts; use bootstrap conservatively.

When adding noise/mitigation features, justify models and cite references in docs.

Performance Contributions
Include before/after benchmarks and complexity analysis.

Consider cache behavior, vectorization, and memory layout; avoid regressions for small/medium n.

Keep density-matrix safeguards (warn on large n) and never compromise correctness.

Security, Privacy & Ethics
Responsible disclosure: see SECURITY.md.
Private reports → AUSP59 alanursapu@gmail.com.

Do not include datasets with personal data or secrets in tests/examples.

No telemetry or tracking. Prometheus metrics are opt-in from outputs.

Dependency Policy
Favor the standard library. New runtime deps require strong justification and security review.

Build-time tooling is OK if widely available and optional.

Legal
License: contributions are accepted under the project’s license (see LICENSE).

DCO (Developer Certificate of Origin): sign each commit:

pgsql
Copiar código
Signed-off-by: Your Name <you@example.com>
Configure via git config user.name/user.email or use -s with git commit.

Communication & Conduct
Be respectful and professional. See Code of Conduct.

Moderation/contact: AUSP59 alanursapu@gmail.com.

Pre-commit (optional but recommended)
bash
Copiar código
pip install pre-commit
pre-commit install
# Hooks: clang-format, clang-tidy (compile_commands), license headers, codespell
Release (maintainers)
Update version in CMakeLists.txt and CHANGELOG.md.

Ensure SBOM, docs/manpages, and schema are current.

Tag vX.Y.Z; CI publishes artifacts (CPack, container, SBOM).

Verify provenance and checksums.

Thank you for making QuantumSimX better. Your expertise helps keep this project world-class and truly reproducible.
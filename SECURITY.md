# Security Policy

> Maintainer & security contact: **AUSP59** <alanursapu@gmail.com>  
> Please read and follow this policy when researching or reporting vulnerabilities.

---

## Supported Versions

We provide security fixes for the latest **major.minor** release line.

| Version line | Status           |
|--------------|------------------|
| `2.x`        | Supported (patches) |
| `< 2.0`      | End of life (EOL)   |

Security fixes are released as patch versions (e.g., `2.1.3`). Older lines may receive fixes at our discretion if impact is critical and backport is low risk.

---

## Reporting a Vulnerability (Coordinated Disclosure)

- **Email (confidential):** **AUSP59** <alanursapu@gmail.com>  
- **GitHub (preferred if available):** Use the repository’s **“Private vulnerability reporting”** feature to open a confidential advisory.

**Response targets**
- Acknowledge receipt: **≤ 48 hours**
- Triage & initial assessment: **≤ 7 days**
- Fix ETA: communicated after triage (aim: **≤ 90 days**), may accelerate for actively exploited issues.

Please include:
- A clear description, impact, and prerequisites
- Reproduction steps (commands, inputs, environment, compiler/OS)
- Proof-of-concept or crash traces (minimal)
- Suggested mitigations (optional)
- Whether you prefer public credit

If you prefer encryption, request a temporary PGP key via email.

---

## Scope & Safe Harbor

**In scope**
- Source code, build scripts, release artifacts, and documentation in this repository
- The **CLI** and **C API** exposed by this project

**Out of scope**
- Denial of Service (DoS), volumetric attacks, rate-limit testing
- Social engineering, phishing, physical security, third-party platforms/services
- Vulnerabilities in dependencies not maintained here (report upstream)
- Findings requiring privileged local access without a clear privilege escalation

**Safe harbor.** We support good-faith research that follows this policy. We will not pursue or support legal action for security research conducted and reported responsibly, without data exfiltration, privacy violations, or service disruption, and within the scope above.

---

## Coordinated Disclosure Process

1. **Report privately** (email or GitHub advisory).  
2. **Triage** (severity via **CVSS v3.1**).  
3. **Fix & verify** (tests, CI, cross-platform builds).  
4. **Release** patched versions and publish a **GitHub Security Advisory (GHSA)**.  
5. **Credit** the reporter (if desired) in the advisory and changelog.

Active exploitation or widespread risk may accelerate timelines. If we cannot meet a reasonable timeline, we’ll provide interim mitigations.

---

## Severity & Remediation

- We use **CVSS v3.1** to prioritize remediation.
- Security releases include:
  - Patched source
  - Updated changelog and advisory
  - Mitigation notes (if config/workarounds exist)

---

## Cryptography & Randomness

- The simulator uses **PCG32** for **deterministic, reproducible simulation**.  
  **It is not a cryptographic RNG.** Do **not** use it for security-sensitive purposes.
- No encryption or transport security is implemented by this project itself. If you embed it, ensure appropriate cryptographic protections at the application layer.

---

## Supply-Chain Security

We strive for strong software supply-chain hygiene:
- **SBOM** (SPDX) shipped in releases where feasible
- **SPDX license identifiers** in sources
- **Static analysis** (e.g., CodeQL), linting, and sanitizers in CI
- **Dependency updates** tracked and reviewed
- **Provenance**: runs record version, input hashes, and configuration (see README)

If you discover a risk in our build, release, or dependency process, please report it as a security issue.

---

## Secrets & Privacy

- No telemetry or tracking in the project.
- Do not commit secrets (tokens, keys). If a secret is exposed:
  - **Revoke/rotate immediately**
  - Open a **private** report with the commit/PR reference
- Test data must not contain personal or sensitive information.

---

## Hardening Guidance (for Users)

- Build with release + hardening flags (RELRO/NOW, stack protector, FORTIFY) as documented in the README/CMake.
- Run untrusted inputs in constrained environments (containers, reduced privileges).
- Prefer state-vector backend for scale; density-matrix has `4^n` memory growth and includes a CLI safety warning.

---

## Vulnerability Report Template

Title: <short, precise>
Product: QuantumSimX
Version/commit: vX.Y.Z (or SHA)
Environment: <OS, compiler, build flags>
Severity (CVSS v3.1 estimate): <vector and score if known>

Summary:
<one-paragraph description>

Reproduction:
<exact commands, inputs, files; minimal PoC>

Impact:
<confidentiality/integrity/availability; realistic scenarios>

Mitigations:
<any known configuration/workarounds>

Reporter name (for credit):
<your preferred display name or Anonymous>

Embargo:
<any requested coordination details>

yaml
Copiar código

---

## Credits & Bounty

We’re happy to credit researchers in advisories and changelogs.  
At this time, **no monetary bounty** is offered.

---

## Contact

**Primary:** **AUSP59** <alanursapu@gmail.com>  
Use English or Spanish. For urgent issues, include “SECURITY” in the subject.

---

## security.txt (optional for sites)

If you operate a website for this project, consider publishing:

/.well-known/security.txt
Contact: mailto:alanursapu@gmail.com
Preferred-Languages: en, es
Policy: https://github.com/AUSP59/QuantumSimX/blob/main/SECURITY.md
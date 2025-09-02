
# Release Playbook
1. Update CHANGELOG and version in `CMakeLists.txt`.
2. Run CI (`ci.yml`, `coverage.yml`, `codeql.yml`, `scorecard.yml`).
3. Build artifacts via CPack (ZIP/TGZ) and sign with Sigstore (optional).
4. Publish docs (Pages) and attach SBOM.
5. Verify schema compatibility and manpage.

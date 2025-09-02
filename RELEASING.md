
# Releasing
1. Update CHANGELOG.md and bump version in CMakeLists.txt.
2. Tag with `vX.Y.Z` and push.
3. CI will build artifacts (ZIP) and attach to release.
4. Optionally sign artifacts with Sigstore if secrets are available.

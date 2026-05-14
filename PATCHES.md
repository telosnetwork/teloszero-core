# Patch Provenance

This file records notable upstream Spring or Spring Community Edition patches
incorporated by TelosZero maintainers outside of normal upstream merges.

## Stable 1.2 Release Line

The TelosZero `release/1.2-teloszero` branch incorporates selected post-1.2.2
Spring fixes for reproducibility, build compatibility, test stability, and
runtime stability. The release branch remains the authoritative source for the
exact commits included in the `teloszero-v1.2.2` release.

Meaningful upstream patch sources for that release include:

- AntelopeIO/spring#1742 - initialize the atomic `timer_state_t`
- AntelopeIO/spring#1841 - validate the pinned Debian package repository
- AntelopeIO/spring#1845 - validate LLVM and CMake tarball hashes
- AntelopeIO/spring#1873 - improve block/producer-round test logic
- AntelopeIO/spring#1875 - avoid undefined behavior in WASM injection
- AntelopeIO/spring#1876 - wait for block 2 in CLI tests
- AntelopeIO/spring#1877 - support multi-threaded subprocess log writes
- AntelopeIO/spring#1879 - signal block processing if sync is interrupted
- AntelopeIO/spring#1881 - avoid OC interrupts during replay

Larger behavior changes should be reviewed in their own TelosZero pull requests
before landing on `main` or a release branch.

# Patch Provenance

This file records bootstrap patches imported after the initial
`AntelopeIO/spring` `main` mirror.

## Bootstrap Cherry-Picks

The following commits were cherry-picked from upstream Spring branches or the
Spring Community Edition history because they were already proposed upstream and
address reproducibility, test stability, or runtime stability:

- `4c510d62d7c94df108c640b8a952d833a8d86d6c` - validate hash of pinned package repo
- `4f325af99cb87ddaad28040ec78f39f58e8672f8` - validate LLVM and CMake tarball hashes
- `4a927beeb8eb04d9d8ca196a8f8ed1f5852fd677` - calculate block in round correctly
- `8521492403d02446a8d422fed1c8ff550dae36d3` - simplify and make production-round test more robust
- `e190658864d4d21565a626e7d8ca38fdd5023e7e` - avoid undefined behavior in WASM injection
- `96fce4255754ad18aa9af008a1e0bcea6752f128` - wait for block 2 in CLI test
- `b6c36a0abd98cfe4b6e71cbc99e410e66d0b6d1b` - support multiple threads writing subprocess logs
- `f4824016fdfabb5ddb48db385ff6037c91d654ad` - hold log filename lock during access
- `6e77b88e8b56183416559b6b679256f9fa786e2c` - signal controller to apply blocks if sync is interrupted
- `96bcb7544e3ac92fb7696cfed9c9052a4ed02af3` - do not OC interrupt during replay

Community Edition release commits and package rename commits were not imported.

# TelosZero Core

**The Telos protocol stack has a home again.**

TelosZero Core is the TelosNetwork-maintained continuation of Antelope Spring:
a focused, Telos-owned chain software line for operators, infrastructure
providers, exchanges, tooling teams, and protocol contributors who need a
serious production path forward.

This is not a casual rename. TelosZero keeps the battle-tested Spring runtime
surface, preserves the history that matters, and moves release authority,
packaging, provenance, and long-term maintenance into Telos hands.

This stable branch is based on upstream Spring `v1.2.2`, with the upstream
MIT license update and a documented set of TelosZero release backports applied.
The runtime command names remain stable: `nodeos`, `cleos`, `keosd`, and
`spring-util`.

TelosZero packages are emitted as `teloszero-core` and `teloszero-core-dev`.
They conflict with the historical Spring/Leap/EOSIO packages because those
packages install overlapping node and CLI binaries.

## Current Status

The latest published release is
[`teloszero-v1.4.1`](https://github.com/telosnetwork/teloszero-core/releases/tag/teloszero-v1.4.1).
The `release/1.4.1-teloszero` branch also contains changes merged after that
tag, including native Ubuntu 24.04 source-build support with LLVM 14 and the
OC runtime. Building the branch therefore produces a different binary from
the published 1.4.1 package. Operators should use the tagged package approved
for their network unless a source build has been separately qualified.

## Repository Remotes

Recommended local remotes:

```bash
git remote -v
```

```text
origin   https://github.com/TelosNetwork/teloszero-core.git
upstream https://github.com/AntelopeIO/spring.git
ce       https://github.com/eos-amsterdam-rnd/antelope_spring_ce.git
```

`origin` is the TelosZero canonical remote. `upstream` is fetch-only for Spring
changes. `ce` is fetch-only for reviewing Community Edition patches.

## Branches

`main` is the development branch and should not be used for production.

Stable branches:

- `release/1.2-teloszero` for the first Telos-compatible production line
- `release/1.4-teloszero` for the 1.4.0 release
- `release/1.4.1-teloszero` for 1.4.1 maintenance and later source-build fixes
- future `release/2.x-teloszero` branches for later protocol work

Historical upstream branches and tags are preserved for auditability.

## Licensing

This branch starts from upstream Spring `v1.2.2` and applies the upstream
`main` license update commit that changed Spring to MIT terms. Older Spring
release branches and tags may carry different license terms. TelosZero
production binaries should be cut only from branches whose base and license
provenance are documented.

The upstream copyright and license notice is preserved in `LICENSE`.

## Build From Source

The published 1.4.1 Linux amd64 operator package passed a clean Ubuntu 24.04
install, linked-library check, and local Telos testnet node startup with a
matching chain ID ([hosted qualification run](https://github.com/telosnetwork/teloszero-core/actions/runs/45715939252)).
Ubuntu 22.04 remains supported. Ubuntu 20.04 is a legacy option whose standard
security support ended in May 2025; use Ubuntu Pro/ESM if it remains deployed.

Native Linux source builds with the OC runtime support LLVM 7 through 11 and
LLVM 14. Ubuntu 24.04 uses its distro LLVM 14 package with Clang 18. On Apple
Silicon, use the developer build below.

Requirements:

- C++20 compiler and standard library
- CMake 3.16+
- LLVM 7 through 11 or LLVM 14 for Linux OC builds
- libcurl 7.40.0+
- git
- GMP
- Python 3
- python3-numpy
- zlib

Clone the 1.4.1 maintenance branch with submodules. The default `main` branch
and the published `teloszero-v1.4.1` tag do not contain the Ubuntu 24.04
LLVM 14 source-build change:

```bash
git clone --recursive --branch release/1.4.1-teloszero \
  https://github.com/TelosNetwork/teloszero-core.git
cd teloszero-core
git rev-parse HEAD
```

Install build dependencies on Ubuntu 22.04:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  git \
  libcurl4-openssl-dev \
  libgmp-dev \
  llvm-11-dev \
  python3-numpy \
  file \
  zlib1g-dev
```

Configure and build:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..
make -j "$(nproc)" package
```

For Ubuntu 24.04 native amd64 source builds with OC enabled:

```bash
sudo apt-get update
sudo apt-get install -y \
  clang-18 cmake git libcurl4-openssl-dev libgmp-dev \
  llvm-14-dev ninja-build python3-numpy zlib1g-dev
cmake -S . -B build/ubuntu24 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_PREFIX_PATH=/usr/lib/llvm-14 \
  -DENABLE_OC=ON -DEOSIO_COMPILE_TEST_CONTRACTS=OFF
cmake --build build/ubuntu24 \
  --target nodeos cleos keosd spring-util --parallel 2
build/ubuntu24/programs/nodeos/nodeos --full-version
```

The hosted [Ubuntu 24.04 native source-build job](https://github.com/telosnetwork/teloszero-core/actions/runs/45722873551)
uses these dependencies and configure flags. Build a Debian package separately
with `cmake --build build/ubuntu24 --target package --parallel 2` if needed.

For legacy Ubuntu 20.04 with Ubuntu Pro/ESM, install `g++-10` and pass the compiler explicitly:

```bash
sudo apt-get install -y g++-10
cmake -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..
```

### Apple Silicon developer build

Use a native arm64 terminal on macOS with the Xcode Command Line Tools and
Homebrew. The build uses checked-in contract artifacts and the `eos-vm`
interpreter. JIT, OC, and CDT contract builds are outside this developer path.

```bash
brew install cmake gmp python
git submodule update --init --recursive
cmake --preset macos-arm64 -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build --preset macos-arm64 --parallel "$(sysctl -n hw.logicalcpu)"
ctest --preset macos-arm64
./build/macos-arm64/programs/nodeos/nodeos --full-version
```

## Tests

From the build directory:

```bash
ctest -j "$(nproc)" -LE _tests
ctest -j "$(nproc)" -L wasm_spec_tests
ctest -L "nonparallelizable_tests"
```

Long-running integration tests are available with:

```bash
ctest -L "long_running_tests"
```

## Reproducible Build

The pinned reproducible Docker build is inherited from Spring and is being
carried forward for TelosZero:

```bash
DOCKER_BUILDKIT=1 docker build -f tools/reproducible.Dockerfile -o . .
```

To reduce memory pressure:

```bash
DOCKER_BUILDKIT=1 docker build \
  --build-arg SPRING_BUILD_JOBS=4 \
  -f tools/reproducible.Dockerfile \
  -o . .
```

## Governance And Security

See:

- `GOVERNANCE.md`
- `LEGAL.md`
- `MAINTAINERS.md`
- `PATCHES.md`
- `SECURITY.md`
- `SUPPORT.md`
- `CONTRIBUTING.md`

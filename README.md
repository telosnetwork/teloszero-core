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

TelosZero is in private release-candidate bootstrap. The goal of this branch is
simple and important: establish a conservative, release-ready 1.2 line that
Telos operators can trust before larger protocol work continues on later
branches.

Do not use it for production until the TelosZero maintainers publish a signed
release and an operator upgrade notice.

What is already in place:

- a clean Spring `v1.2.2` base for the first TelosZero production line
- Telos-controlled package identity: `teloszero-core` and `teloszero-core-dev`
- curated backports for release safety, build integrity, and operator hygiene
- documented licensing and patch provenance
- preserved upstream history for auditability
- a path to Telos-owned CI, release signing, package publishing, and support

What comes next:

- cut signed release artifacts from this stable branch
- publish clear operator upgrade guidance
- finish Linux package validation and reproducible build checks
- continue reviewing useful Spring and Spring Community Edition patches
- reserve future `release/2.x-teloszero` work for larger protocol evolution

TelosZero exists to make Telos infrastructure boring in the best possible way:
owned, reviewable, reproducible, and maintained.

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

Use Ubuntu 22.04 or Ubuntu 20.04. macOS and other Unix-like systems are
best-effort only.

Requirements:

- C++20 compiler and standard library
- CMake 3.16+
- LLVM 7 through 11 for Linux OC builds
- libcurl 7.40.0+
- git
- GMP
- Python 3
- python3-numpy
- zlib

Clone with submodules:

```bash
git clone --recursive https://github.com/TelosNetwork/teloszero-core.git
cd teloszero-core
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

For Ubuntu 20.04, install `g++-10` and pass the compiler explicitly:

```bash
sudo apt-get install -y g++-10
cmake -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..
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

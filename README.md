# TelosZero Core

```text
 _______    _            ______
|__   __|  | |          |___  /
   | | ___ | | ___  ___    / / ___ _ __ ___
   | |/ _ \| |/ _ \/ __|  / / / _ \ '__/ _ \
   | |  __/| | (_) \__ \ / /_|  __/ | | (_) |
   |_|\___||_|\___/|___//_____\___|_|  \___/
                         CORE
```

TelosZero Core is the Telos Foundation-maintained continuation of Antelope Spring.
It provides node software and supporting tools for Telos operators,
infrastructure providers, exchanges, tooling teams, and protocol contributors.

TelosZero preserves Spring runtime compatibility while moving release
authority, packaging, provenance, and long-term maintenance under Telos
stewardship.

The runtime command names remain stable: `nodeos`, `cleos`, `keosd`, and
`spring-util`.

## Features And Benefits

TelosZero Core carries forward the operational strengths of the Antelope/Spring
software line while giving Telos a dedicated maintenance and release path.

- **Antelope-compatible runtime:** preserves the familiar `nodeos`, `cleos`,
  `keosd`, and `spring-util` command surface for operators and tooling teams.
- **Savanna consensus support:** inherits Spring's Savanna consensus work for
  modern Antelope-based network operation.
- **High-performance smart contract execution:** includes the EOS VM runtime
  stack used by Antelope chains for WebAssembly contract execution.
- **Operator and exchange APIs:** includes chain, net, producer, state history,
  trace, and HTTP plugin surfaces used by infrastructure providers, exchanges,
  and indexers.
- **Telos-ready genesis materials:** includes public Telos mainnet and testnet
  genesis references for node setup and verification.
- **Conservative release provenance:** starts from known Spring release bases,
  documents selected upstream fixes, and keeps production releases separate
  from forward-development work.
- **Telos stewardship:** moves release ownership, packaging, validation, and
  long-term maintenance into the Telos ecosystem.
- **Future protocol roadmap:** keeps Telos-specific protocol development on
  `main`, including design-reviewed work toward quantum-safe Telos
  infrastructure.

## Status And Branches

`main` is the TelosZero Core development branch. Do not run `main` directly in
production.

Production operators should use tagged releases from maintained release
branches. The first public TelosZero release line is:

- `release/1.2-teloszero` for the conservative 1.2 production line based on
  Antelope Spring `v1.2.2`

The initial public release is
[`teloszero-v1.2.2`](https://github.com/telosnetwork/teloszero-core/releases/tag/teloszero-v1.2.2).

Future development on `main` is where Telos-specific protocol work will be
reviewed, including roadmap work toward quantum-safe Telos infrastructure.
Protocol-level changes must land through explicit design review and release
planning before operator use.

The branch and release structure is intended to keep production releases
reviewable, reproducible, and maintained.

## Binary Installation

Download the operator package from the
[latest TelosZero Core release](https://github.com/telosnetwork/teloszero-core/releases/latest).

For `teloszero-v1.2.2`, the public operator artifact is:

```text
teloszero-core_1.2.2_amd64.deb
```

Install the downloaded package on a supported Ubuntu host:

```bash
sudo apt-get update
sudo apt-get install -y ~/Downloads/teloszero-core_*.deb
```

Your download path may vary. If you are in an Ubuntu container, omit `sudo`
because you run as `root`.

Verify the install:

```bash
nodeos --full-version
```

## Supported Operating Systems

TelosZero Core supports the same primary operating systems inherited from the
Spring line unless a release note says otherwise:

- Ubuntu 22.04 Jammy
- Ubuntu 20.04 Focal

Other Unix-like systems, including macOS, are best-effort only.

## Build From Source

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

## Repository Remotes

Recommended local remotes:

```text
origin   https://github.com/TelosNetwork/teloszero-core.git
upstream https://github.com/AntelopeIO/spring.git
ce       https://github.com/eos-amsterdam-rnd/antelope_spring_ce.git
```

`origin` is the TelosZero canonical remote. `upstream` is fetch-only for Spring
changes. `ce` is fetch-only for reviewing Spring Community Edition patches.

## Governance And Security

See:

- `GOVERNANCE.md`
- `LEGAL.md`
- `MAINTAINERS.md`
- `PATCHES.md`
- `SECURITY.md`
- `SUPPORT.md`
- `CONTRIBUTING.md`

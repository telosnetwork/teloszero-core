# ML-DSA-65 signature-envelope probe

This standalone experiment checks the signature primitive proposed in
[PROPOSAL.md](PROPOSAL.md). It does not modify TelosZero Core, authorize a
transaction, or make a chain quantum resistant.

## Run

Requires OpenSSL 3.5 or newer development headers and library. The source
creates an ephemeral key in memory; it writes no private key or signature to
disk and prints only sizes and test status.

```bash
c++ -std=c++17 -Wall -Wextra -Werror \
  -I/path/to/openssl/include \
  tools/quantum-lab/mldsa65_probe.cpp \
  -L/path/to/openssl/lib -Wl,-rpath,/path/to/openssl/lib \
  -lcrypto -o /tmp/mldsa65_probe
/tmp/mldsa65_probe
```

On OpenSSL 3.6.4, the probe passed with a 1,952-byte public key, a
3,309-byte signature, 5,261 bytes for the bare public-key/signature
payload, and 5,262 bytes with the proposed one-byte variant-3 tag. This
excludes transaction headers and action payloads. The probe checks a
fixed-width tagged round trip and rejects wrong tags, truncation, and
trailing bytes; the actual `fc::raw` serializer still needs a golden-byte
test. The program signs a 32-byte sample digest
using pure ML-DSA with a candidate context string
`TelosZero-QuantumLab-TX-v1`. It verifies the signature through a
public-key-only import and rejects a changed digest, changed context,
and changed signature.

## Pinned BoringSSL API check

TelosZero Core 1.2.2 pins BoringSSL submodule commit
`2587c4974dbe9872451151c8e975f58567a1ce0d`. The separate
`tools/quantum-lab/mldsa65_boringssl_probe.cpp` calls that revision's
`MLDSA65_generate_key`, `MLDSA65_sign`, `MLDSA65_parse_public_key`,
and `MLDSA65_verify` directly. It checks the same domain context and
negative cases, including truncated public-key parsing. The source can
be compiled against the repository's initialized BoringSSL submodule
after building its `crypto` target:

```bash
cmake -S libraries/libfc/libraries/boringssl/bssl -B /tmp/telos-bssl-build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/telos-bssl-build --target crypto -j 4
c++ -std=c++17 -Wall -Wextra -Werror \
  -Ilibraries/libfc/libraries/boringssl/bssl/include \
  tools/quantum-lab/mldsa65_boringssl_probe.cpp \
  /tmp/telos-bssl-build/libcrypto.a -o /tmp/telos-bssl-probe
/tmp/telos-bssl-probe
```

The probe compiled and passed on macOS arm64 against the exact pinned
BoringSSL commit. This is an API and primitive check, not a TelosZero
build, transaction acceptance test, or consensus result.

These results fix the approximate size and API behavior to evaluate during
the protocol design. They do not establish consensus determinism, correct
transaction-digest construction, CPU/NET billing, historical replay,
wallet support, producer/finalizer security, or deployment.

OpenSSL's documented ML-DSA context parameter is at most 255 bytes and
its normal message encoding is the FIPS 204 pure-signature form. The
protocol branch must specify its exact context and message bytes before
the first shared lab activation:

- https://docs.openssl.org/3.6/man7/EVP_SIGNATURE-ML-DSA/
- https://docs.openssl.org/3.6/man7/EVP_PKEY-ML-DSA/
- https://csrc.nist.gov/pubs/fips/204/final

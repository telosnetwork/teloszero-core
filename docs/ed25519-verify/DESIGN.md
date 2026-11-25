# ED25519_VERIFY: Ed25519 signature verification for contracts

## Summary

A new protocol feature, `ED25519_VERIFY`, adds one host function:

```cpp
// 0 if `sig` is a valid Ed25519 signature of `msg` by `pub`, -1 otherwise
int32_t ed25519_verify(const char* msg, uint32_t msg_len,
                       const char* sig, uint32_t sig_len,
                       const char* pub, uint32_t pub_len);
```

Ed25519 (RFC 8032, pure Ed25519) is what Solana wallets, many other chains and a lot of off-chain systems sign
with. Contracts can already check secp256k1 (`k1_recover`) and BLS12-381, but not Ed25519. Doing it in WASM costs
milliseconds of CPU per signature, which is too much for anything that verifies signatures routinely: bridges and
light clients for Ed25519 chains, signed off-chain messages, oracles.

## Semantics

- `sig` must be 64 bytes (R || s), `pub` 32 bytes, `msg` at most 64 KiB
  (`config::ed25519_verify_max_message_size`). Anything else returns -1; the call never throws on bad input.
- Verification is strict, as libsodium's `crypto_sign_ed25519_verify_detached`:
  - s must be canonical (s < L), so signatures are not malleable;
  - the public key must be a canonical encoding;
  - neither R nor the public key may be a point of small order;
  - the equation is checked without the cofactor.
- The result is deterministic on every node: libsodium is pinned (see below), never taken from the OS.

## Implementation

- **libsodium** 1.0.22-RELEASE (commit `77e1ce5d6dee871c49ef211222ba18ef0c486bda`) is a submodule at
  `libraries/libfc/libsodium/libsodium`, built out of tree with its own `configure` through `ExternalProject`
  (`--disable-shared --enable-static --with-pic`) and linked privately into `fc`. No public fc header includes
  `sodium.h`.
- **fc**: `fc/crypto/ed25519.hpp` with `fc::ed25519::verify`, plus `sign` and `public_key_from_seed`, which only
  tests and tools use.
- **chain**: `interface::ed25519_verify` in `webassembly/crypto.cpp`, registered for eos-vm and appended at the end
  of the OC intrinsic table (existing indices do not move).
- **Protocol feature**: `builtin_protocol_feature_t::ed25519_verify = 500000`, the first value of the range Spring
  reserves for forks (`reserved_private_fork_protocol_features`). Upstream already uses 25-27 on its 2.0 line, so
  a value from the main range would collide on the next merge. The activation handler whitelists the intrinsic; no
  dependencies.

## CPU and limits

The host function is metered by time like every other one, and `checktime` runs after the call. libsodium hashes
the whole message with SHA-512 in one call that `checktime` cannot interrupt, so messages are capped at 64 KiB
(about 0.1 ms of SHA-512).

## Compatibility

- Consensus change only after activation. Until block producers activate `ED25519_VERIFY`, nodes with and without
  this patch agree: contracts that import `ed25519_verify` fail `setcode` with `env.ed25519_verify unresolveable`,
  as for any unknown intrinsic.
- After activation, nodes without the patch cannot validate blocks with contracts that call it, as with any
  protocol feature: all producers and validators must upgrade before activation.
- No storage, snapshot or state-history format changes. `nodeos`, `cleos` and `keosd` interfaces are unchanged.
- Contracts import it directly (`eosio_wasm_import`), CDT needs no change.

## Alternatives considered

- **BoringSSL** is already vendored and has `ED25519_verify`. This patch uses libsodium because its acceptance rules
  are documented and strict (small-order keys and R rejected), and it is what the implementation was developed and
  tested against. Moving to BoringSSL would change which edge-case signatures are accepted, so it would need its
  own review of the rules.
- **Verification in WASM** works today without a protocol change, but costs orders of magnitude more CPU.

## Tests

`unittests/ed25519_verify_tests.cpp`:

- before activation the import is unresolveable;
- RFC 8032 section 7.1 vectors 1-3 verify; a changed signature, message or key fails;
- a non-canonical s (s + L) and a small-order public key are rejected;
- wrong signature and key sizes return -1;
- messages of 1000 bytes and of exactly 64 KiB verify; 64 KiB + 1 returns -1 even with a valid signature.

`protocol_feature_digest_tests` checks the description digest of the new feature.

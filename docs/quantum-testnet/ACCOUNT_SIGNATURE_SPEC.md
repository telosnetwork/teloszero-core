# Quantum Lab account-signature protocol sketch

Status: design for a dedicated lab fork. No consensus implementation or chain activation is in this PR. This document pins the wire proposal and the source changes needed before deployment.

## Scope and fixed choices

The first protocol experiment adds **ML-DSA-65 account authorization** only. Keep producer K1 signatures and Savanna BLS finality unchanged and describe the result accordingly. The signature input is the existing 32-byte `transaction::sig_digest(chain_id, context_free_data)` result. Sign those exact 32 bytes with **pure ML-DSA-65** and the byte string `TelosZero-QuantumLab-TX-v1` as the FIPS 204 context. Do not prehash the digest again or use HashML-DSA. The transaction digest already commits to the chain ID, packed transaction, and context-free-data hash.

The proposal reserves variant index **3** after K1 (0), R1 (1), and WebAuth (2) for both public keys and signatures. Existing indices and their encodings must remain byte-for-byte unchanged. Use a fixed 1,952-byte raw ML-DSA-65 public key. The signature payload carries that same public key followed by a fixed 3,309-byte raw signature; this lets the verifier return the authenticated public key to the existing authority matcher without elliptic-key recovery.

| Object | Proposed binary representation | Bytes |
| --- | --- | ---: |
| Account public key | canonical variant tag 3, then 1,952 raw bytes | 1,953 |
| Transaction signature | canonical variant tag 3, then 1,952 raw public-key bytes, then 3,309 signature bytes | 5,262 |
| Signature payload alone | public key and signature, without the variant tag | 5,261 |

These counts assume the `fc::raw` variant index uses its existing canonical one-byte varuint32 encoding for 3, and fixed arrays pack without a length prefix. Check these assumptions with golden wire tests against the exact implementation. Do not use length-prefixed vectors here. JSON/base58 prefixes such as `PUB_MLDSA65_` and `SIG_MLDSA65_` are proposed human encodings; define their checksum and canonical round-trip rules before client use. The signature may be randomized, so golden tests should compare verification and byte layout, not require a unique signature for a message.

## Verification rule

1. Reject any index 3 signature before the **Quantum Lab only** activation feature. Do not rely solely on the authority key-type gate: `transaction::get_signature_keys` currently invokes `public_key(sig, digest)` before authority checking, and authorization checks key-type activation separately.
2. Enforce the exact 5,261-byte payload and 1,952/3,309 component lengths during unpacking, before public-key import or cryptographic work. Reject trailing bytes at the signed-transaction boundary and noncanonical variant encodings.
3. Import only the embedded raw public key as ML-DSA-65. Reject an invalid parameter set or malformed import. Verify the raw 3,309-byte signature over the exact 32-byte `sig_digest` using the context above. Treat any OpenSSL error as invalid; do not fall back to another algorithm, context, or provider.
4. Only after verification, construct a variant-3 `public_key` from the embedded bytes and insert it into the recovered-key set. The existing duplicate-key rule then applies to two signatures with the same embedded key, including distinct randomized signatures.
5. Match that key against activated account authorities. A K1 or R1 threshold path that can authorize alone remains a classical bypass; tests must distinguish ML-DSA-only from mixed authorities.

The verification function must be deterministic across independently built nodes. Pin the crypto implementation, version, provider selection, build flags, and FIPS 204 test vectors. OpenSSL 3.5+ provides the standalone probe API, but TelosZero's top-level `CMakeLists.txt` explicitly disables external OpenSSL because the node links bundled BoringSSL (`libraries/libfc/CMakeLists.txt`). Do not link the probe's OpenSSL implementation directly into nodeos: that can create conflicting crypto symbols and providers. The release's pinned BoringSSL submodule revision `2587c4974dbe9872451151c8e975f58567a1ce0d` already declares `MLDSA65_sign`, `MLDSA65_verify`, `MLDSA65_public_key_from_bytes`, the 1,952/3,309-byte constants, and context parameters in `include/openssl/mldsa.h`. Prefer this pinned implementation for the lab fork, with direct FIPS 204 vectors and cross-node checks; do not pull in a second OpenSSL library. The Ubuntu package currently pinned for private bootstrap has no ML-DSA account support and must be rebuilt from that lab fork before protocol testing. See https://boringssl.googlesource.com/boringssl/+/2587c4974dbe9872451151c8e975f58567a1ce0d/include/openssl/mldsa.h. Never silently fall back to the host's default OpenSSL.

## Required source changes

| Area | Existing path and behavior | Lab-fork change |
| --- | --- | --- |
| Variant and codecs | `libraries/libfc/include/fc/crypto/{public_key,signature,private_key}.hpp` and `libraries/libfc/src/crypto/{public_key,signature,private_key}.cpp` define K1/R1/WA variants, string forms, visitors, and signing. | Append ML-DSA-65 at index 3; implement bounded fixed serialization, public-key import, signing, verification, hashing/comparison, and human encoding without changing earlier variants. |
| Key extraction | `libraries/chain/transaction.cpp::transaction::get_signature_keys` calls `recovered_pub_keys.emplace(sig, digest)`; `public_key.cpp` visits `sig.recover(digest)`. | Verify embedded-key ML-DSA and return the authenticated key. Preserve the old recovery path for K1/R1/WA. Gate index 3 by feature state before this path. |
| Feature state | `controller.cpp::on_activation<webauthn_key>` raises `num_supported_key_types` to 3; `authorization_manager.cpp` rejects unactivated authority keys. | Add a distinct, lab-only built-in feature that raises the count to 4 and gates signatures too. Specify replay behavior at the activation block. |
| Limits and billing | `transaction_metadata.cpp` checks `signature::variable_size()`; `chain_plugin.cpp` defaults `maximum-variable-signature-length` to 16,384. Signature work is timed in `get_signature_keys`. | Treat the 5,261-byte signature payload and verification work in objective NET/CPU accounting. Test maximum signature count, packed transaction size, block limits, deadline behavior, and producer subjective limits. Do not set `variable_size()` to zero merely to bypass a limit. |
| WASM host API | `libraries/chain/webassembly/crypto.cpp` recovers a key from a signature and checks `num_supported_key_types`. | Define whether `recover_key` returns the embedded verified key for index 3 and test exact buffer-size behavior, or introduce an explicit verify intrinsic with a feature gate. Do not expose inconsistent contract behavior. |
| Clients and persistence | cleos, keosd, wallet APIs, ABI/JSON codecs, snapshots, and history consume serialized keys/signatures. | Add import/export, signing and display, replay, snapshot restart, and client round-trip tests before multi-node operation. |

## Acceptance tests before any shared activation

- Golden encodings for old K1/R1/WA and new index 3; old blocks replay identically.
- Valid ML-DSA-only authority accepts; wrong chain ID, changed transaction/CFD, changed context, wrong public key, bad signature, and malformed/truncated/oversized encodings reject.
- Index 3 rejects before activation on all transaction and authority paths; all nodes agree at the activation block and after restart.
- Duplicate randomized signatures from the same embedded key reject under existing duplicate-key policy.
- Mixed authority cases show exactly which threshold paths remain classical.
- Independent builds agree on acceptance, state roots, NET accounting, CPU-limit policy, and fork/replay results. Record observed verification time separately because wall-clock CPU measurements vary. Include fuzzing around unpack boundaries and key import.
- Live lab blocks and LIB progress only after system contracts and Savanna finalizer setup are independently validated.

The standalone probe in `tools/quantum-lab/mldsa65_probe.cpp` exercises key generation, public-only import, and primitive verification only. It is not a substitute for the above consensus tests.

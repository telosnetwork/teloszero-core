# Telos Zero Quantum Lab: isolated network and protocol experiment

Status: design proposal. No network has been launched by this document, and current
Telos Zero binaries do not accept post-quantum account keys.

## Purpose and boundaries

Create a new Telos Zero chain for experiments with post-quantum account
authorization. It is a separate network, not a reset, fork, or upgrade of
Telos mainnet or the public testnet. The first experiment is ML-DSA-65 account
authorities and transaction signatures. "Quantum Lab" is a research network
name, not a claim that the whole chain is quantum resistant.

The initial deployment is Zero-only. Do not connect Telos EVM execution,
the Zero/EVM bridge, LayerZero, production wallets, or any real asset route
until each integration has a separate design and test result. Chain ID 40/41,
the existing Zero mainnet/testnet chain IDs, their genesis files, accounts,
keys, snapshots, and peer lists are out of scope for reuse.

## Network identity and isolation

1. Pin an exact TelosZero Core source commit, reproducible Ubuntu x86_64
   package, checksum, and contract artifacts in a release manifest.
2. Derive a fresh genesis from the pinned release's configuration values,
   with a new UTC genesis timestamp and a newly generated lab-only K1 genesis
   public key. The serialized genesis determines the Antelope chain ID; the
   JSON file's SHA-256 is not the chain ID.
3. Start from empty data directories. Record the chain ID reported by
   `/v1/chain/get_info` and reject the known Telos Zero mainnet
   (`4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11`)
   and testnet
   (`1eaa0824707c8c16bd25145493bf062aecddfeb56c736f6ba6397f3195f33c9f`)
   IDs. Every node and client must pin the new ID.
4. Use dedicated hosts or isolated instances, data, logs, service users,
   firewall rules, DNS, API/P2P ports, monitoring, and lab-only signer keys.
   Begin with loopback-only API/P2P for bootstrap; publish only after the
   multi-node admission checks below.
5. Seed peers only from an explicit lab allowlist. Never use mainnet/testnet
   snapshots, peer discovery output, or `p2p-peer-address` values. Reject
   a peer handshake whose chain ID differs from the lab manifest.
6. Keep the producer management API private. Store keys outside Git and
   outside command arguments; use restrictive file permissions or an
   operator-controlled signing provider. Never use a mainnet/testnet account,
   producer, or finalizer key.
7. Bootstrap the exact system contracts and protocol features required by
   the pinned core release. The local one-node development recipe is only a
   smoke test; it does not establish Savanna finality. Document feature
   activation, producer schedule, finalizer policy, BLS key custody, and
   `safety.dat` handling before calling the multi-node network live.

## Protocol experiment: ML-DSA account authorization

The core currently serializes K1, R1, and WebAuth key/signature variants.
Authorization obtains signing keys by recovering public keys from signatures.
ML-DSA is not a recoverable-signature scheme. A consensus change must therefore
define a new, versioned key type and a bounded signature envelope that carries
the public key, with deterministic verification against the transaction digest.
The digest already includes the chain ID; specify additional domain separation
and canonical encoding so the same bytes cannot be accepted under another
scheme or context.

Use the final NIST FIPS 204 definition of ML-DSA-65. Pin a reviewed
implementation and test vectors. Reject malformed lengths, noncanonical
encodings, wrong parameter sets, wrong digest/context, invalid signatures,
and oversized envelopes before expensive verification. Charge deterministic
CPU/NET resources for key, signature, and verification work. Define limits
for transaction size, authority storage, API responses, ABI/JSON encoding,
wallet import/export, and hardware signing before activation.

Implementation surfaces to review together:

- `libraries/libfc` public/private key and signature variants,
  serialization, parsing, signing, and verification;
- `libraries/chain` transaction signature-key extraction, authorization,
  protocol feature activation, validation, resource billing, and persistence;
- `programs/cleos`, `programs/keosd`, wallet APIs, ABI handling, and
  SDK/client encoding;
- WASM signature-verification host functions if contracts need them;
- API, explorer, and wallet display of the new key type.

Preserve historical K1/R1/WebAuth decoding and replay. Activate the new type
only at a documented feature boundary after all lab nodes run the same
binary. Exercise both ML-DSA-only and mixed classical/ML-DSA authorities.
A mixed authority is not automatically post-quantum secure: if a classical
threshold path alone can authorize, the account remains exposed. Producer
K1 and Savanna BLS signatures remain classical in this first experiment, so
passing account-signature tests must not be described as whole-chain quantum
resistance.

## Deployment stages and evidence gates

| Stage | Result required to advance |
| --- | --- |
| A. Source baseline | Pinned core and system-contract commits, package hashes, operator topology, threat model, and reproducible tests. |
| B. Private chain | New genesis and chain ID; two clean restarts; block head and irreversible block advance; no connection to existing Telos networks. |
| C. Protocol branch | Positive and negative ML-DSA vectors; activation-boundary, mixed-authority, replay, fork/state-root, persistence, malformed-input, and CPU/NET tests; matching results on independently built nodes. |
| D. Multi-node lab | At least three independently operated nodes; documented producer/finalizer setup; API/P2P isolation; continuous head/LIB progress; restart and one-node-failure recovery. |
| E. Public testbed | Separate DNS, faucet for valueless lab tokens, explorer/client chain-ID pinning, published binary/genesis/contract hashes, operator runbook, monitoring and incident/rollback process. |

Track signature and public-key byte sizes, verification latency and CPU
billing, transaction acceptance/rejection, block size/throughput, finality,
replay results, node restart recovery, and client compatibility. Store raw
logs and exact binaries with each result. A source test, local single-node
smoke, or open PR is not evidence that stage E is deployed.

## Cross-layer policy

Keep Telos EVM and bridges disabled initially. A later EVM experiment needs
its own chain ID (not 40 or 41), execution/consensus configuration, genesis,
RPC, transaction signer compatibility, and Zero/EVM asset accounting. Existing
Ethereum ECDSA accounts remain classical even if Zero account authorization
adds ML-DSA. A separate threat model and demonstrated end-to-end tests are
required before any claim about EVM interoperability or post-quantum assets.

## External references and attribution

XPR Network's public testnet uses a separate genesis and chain ID, which is a
useful operational separation pattern:
https://github.com/XPRNetwork/xpr-testnet.start
https://docs.xprnetwork.org/getting-started/mainnet-vs-testnet

MetalBlockchain/pulsevm PR #116 proposes ML-DSA account keys, transaction
signing, and WASM intrinsics, but was open when this proposal was written.
PulseVM is a different protocol; this proposal does not assume that code is
merged, deployed on XPR, or portable to TelosZero Core:
https://github.com/MetalBlockchain/pulsevm/pull/116

NIST FIPS 204: https://csrc.nist.gov/pubs/fips/204/final

## Decisions still needed before host deployment

- Lab host(s), capacity, operators, and recurring infrastructure budget.
- Whether stage B is private to the Telos team or stage E will be public,
  and the desired DNS naming.
- Key custody and the review owners for protocol, contracts, wallet, and
  Savanna finality.
- Exact protocol release branch, activation plan, and artifact pipeline.

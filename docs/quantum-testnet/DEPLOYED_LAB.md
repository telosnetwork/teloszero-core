# Telos Zero ML-DSA-65 Quantum Lab deployment

Observed on 2026-10-22 UTC. This is an experimental, isolated chain on Servarica service 88875, separate from Telos mainnet, the public Telos testnet, and the earlier classical lab chain on the same VM.

## Identity and isolation

- Chain ID: `01fd49dc25b64a7288ac34a0145b5581882dfac0c4e2bfadc7f3200ef4b5d99f`
- Producer: `teloszero-pq-experiment.service`, API `127.0.0.1:8892`, P2P `127.0.0.1:9892`.
- Observer: `teloszero-pq-observer.service`, API `127.0.0.1:8893`, P2P `127.0.0.1:9893`, with only the producer as its loopback peer.
- Fresh genesis, producer key, config, and data under `/var/lib/teloszero-quantum/pq-experiment` and `/var/lib/teloszero-quantum/pq-observer`. Both services run as `quantumlab`; all four ports are bound to loopback.
- Pinned experimental node executable: `/opt/teloszero-quantum/bin/nodeos-pq`, SHA-256 `6b1695ba48984271ac43e581dd7eeb2b852f9f9506017d7891d03db138ccae64`. The producer launcher verifies its hash and genesis chain ID before exec.
- The older classical lab chain keeps its packaged `/usr/bin/nodeos` binary, chain ID `f7e7dbe3b350774870728937c79a6c8e45861d30eed12f4bfd1d05e9a8cc22b4`, and ports 8890/8891/9890/9891.

## Implemented experiment

The lab fork adds ML-DSA-65 as account public-key and transaction-signature variant 3. A public key is 1,952 bytes and a signature contains an authenticated embedded key plus 3,309 signature bytes. The signature covers the existing transaction `sig_digest` with context `TelosZero-QuantumLab-TX-v1`, using the source tree's pinned BoringSSL. Seed-based private-key import/export, wallet signing, and `cleos create key --mldsa65` are present. Variant 3 signatures are accepted only when the transaction chain ID equals this lab chain ID; the lab genesis sets `num_supported_key_types` to 4, while other chain IDs retain their prior key limits. Existing K1 producer signatures and BLS finality are unchanged.

This exact-chain-ID gate is a temporary lab mechanism. A general protocol-feature activation, upgrade/replay policy for other chains, cross-version peer compatibility, broad fuzzing, objective resource calibration, and independent build qualification remain future work. Do not use this fork on Telos mainnet or public testnet, or describe the whole chain as quantum resistant.

## Evidence

- The unmodified checkout built `nodeos` on Ubuntu 22. The patched checkout built `test_fc`, `eosio_chain`, `nodeos`, `cleos`, and `keosd`.
- Focused ML-DSA tests passed fixed-size wire round trips, seed-key signing, digest/signature/key alteration rejection, and truncated input rejection. A 33-case run covering K1 recovery, WebAuth, and ML-DSA passed.
- Account `qmlpqprobeaa` has owner and active authorities containing only a `PUB_MLDSA65_` key on both nodes.
- Transaction `46ef395bd2709ca14084abe830639db6a7ac9a2998869a6a0416c6d5ec35d888` contains one `SIG_MLDSA65_` signature and executed `eosio::updateauth` in block 674. Both nodes later reported LIB 964 and the same block 674 ID `000002a2638046a4ee3397eca08c92c52c47962dae30accf17f06fdb2513e3d7`; `pqaudit` permission appeared on both.
- A transaction signed with the older classical lab chain ID was rejected with `invalid ML-DSA-65 transaction signature`; its attempted `pqneg` permission remained absent on both nodes.
- Producer and observer were restarted separately; both resumed block production/synchronization and returned the same chain ID, LIB, block 674 ID, and account authority state. The temporary signing wallet was stopped afterward.

## Operator access

SSH: `ssh -i ~/.ssh/id_ed25519 root@38.49.218.236`. To query the private PQ API from a Mac, use `ssh -N -L 8892:127.0.0.1:8892 -i ~/.ssh/id_ed25519 root@38.49.218.236` and POST `{}` to `http://127.0.0.1:8892/v1/chain/get_info`. Keep the API and P2P ports private until access controls and abuse limits are designed.

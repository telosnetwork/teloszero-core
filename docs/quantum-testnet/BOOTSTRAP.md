# Private Quantum Lab bootstrap (stage B)

This runs a new, isolated Telos Zero chain for baseline and restart tests.
It uses ordinary K1 signing. It does **not** implement ML-DSA or Savanna
finality. See [PROPOSAL.md](PROPOSAL.md) before multi-node or public use.

## Prerequisites

- Dedicated Ubuntu 22.04 x86_64 lab host with sufficient disk and RAM,
  access restricted to the lab operators, Python 3, and the tagged
  `teloszero-v1.2.2` TelosZero Core package. Verify its SHA-256 is
  `285fdfc1abde5104892d94b1f380c6d79aba35eac3413f139119ea88574c5007`.
- Fresh lab-only K1 keypair generated in the operator's key custody.
  Never use a mainnet/testnet account, producer, or finalizer key.
- A new directory outside every existing Telos data/config path.
  There is no snapshot import and no external P2P peer at this stage.

The script embeds the `initial_configuration` from the tagged release's
`genesis/telos_testnet_genesis.json` (file SHA-256:
`bfd385a8c5618ea86d6ce3091339d567c0d03fe1858810a5f64586f562769720`).
It replaces the timestamp and initial public key. The resulting serialized
genesis yields a different chain ID. The script checks the observed ID
against both existing Telos Zero chain IDs.

## Prepare

On the isolated host, after verifying the binary and generating a fresh
K1 keypair:

```bash
sudo install -d -m 0700 -o "$USER" -g "$(id -gn)" /var/lib/telos-quantum-lab
# Prepare requires a path that does not already exist:
python3 scripts/quantum-lab/bootstrap.py prepare \
  /var/lib/telos-quantum-lab/private-chain \
  --public-key 'PUB_K1_LAB_ONLY_PUBLIC_KEY' \
  --timestamp "$(date -u +'%Y-%m-%dT%H:%M:00.000')"
```

The created `config/config.ini` binds API and P2P to loopback at ports
8890 and 9890. Add exactly one lab-only
`signature-provider = PUB_K1_...=KEY:PVT_K1_...` line to that private
config, matching the genesis public key. Keep the config mode 0600 and
accessible only to the node service user. Do not paste the private key into
a ticket, terminal command, Git, or logs.

## First start and checks

Start `nodeos` on the lab host with the verified package. Keep it in the
foreground during initial inspection. The data directory must be empty:

```bash
nodeos \
  --config-dir /var/lib/telos-quantum-lab/private-chain/config \
  --data-dir /var/lib/telos-quantum-lab/private-chain/data \
  --genesis-json /var/lib/telos-quantum-lab/private-chain/genesis.json
```

In another terminal, confirm that port 8890 belongs to this `nodeos`
process and its command names the intended config/data paths. Then run:

```bash
python3 scripts/quantum-lab/bootstrap.py pin \
  /var/lib/telos-quantum-lab/private-chain
python3 scripts/quantum-lab/bootstrap.py check \
  /var/lib/telos-quantum-lab/private-chain
```

`pin` is one-time only. It stores the API-reported chain ID after rejecting
both existing Telos IDs. `check` requires the pinned ID, an advancing head,
and a visible irreversible block. Repeat `check` after two clean node
restarts. On subsequent starts, omit `--genesis-json`; never use
`--delete-all-blocks` on a running or retained lab.

This is a private development smoke test. Before a multi-node launch,
build exact system contracts, document activation and finalizer policy,
use independent producer/finalizer keys, and follow the stages in the
proposal. Do not point a public DNS name, wallet, EVM node, or bridge at
this chain based on a passing smoke test.

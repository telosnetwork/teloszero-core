---
content_title: Upgrade a Non-Producing Node from Antelope Leap
---

## Who This Is For

This guide is for operators of **non-producing (non-BP) nodes** — exchanges,
API providers, wallet and explorer backends — who are running **Antelope
Leap** and want to move to **TelosZero Core `teloszero-v1.2.2`**.

This is a **major upgrade** (Leap → the Spring 1.2 line). Unlike a same-line
package swap, the on-disk chain state from Leap is **not** loaded in place. The
supported path is to **restore from a snapshot** onto a fresh state directory.
TelosZero reads Leap snapshots directly, so this is straightforward:

- The command names are unchanged: `nodeos`, `cleos`, `keosd`.
- Plugin names are unchanged (still `eosio::...`).
- `leap-util` is now named **`spring-util`**.

There is **no full replay from genesis** — you migrate via a snapshot.

## Before You Begin

- Check your current version: `nodeos --full-version`.
- Supported OS: Ubuntu 22.04 (Jammy) or Ubuntu 20.04 (Focal).
- Have your `config.ini` handy — one option must be removed (see step 3).
- **Snapshot compatibility:** TelosZero Core 1.2.2 loads chain snapshots of
  versions 2–8. Leap 5.0 produces v6 snapshots and Leap 4.x produces v5, so a
  snapshot from a current Leap node restores directly.

## Upgrade Steps

### 1. Take a snapshot on your Leap node

A snapshot is how you carry state across the upgrade. It requires the
`producer_api_plugin`. If it isn't already enabled, add it (for example
`plugin = eosio::producer_api_plugin` in `config.ini`, or
`--plugin eosio::producer_api_plugin` on the command line) and restart Leap
long enough to take the snapshot; you can remove it again afterward.

With Leap running:

```bash
curl -X POST http://127.0.0.1:8888/v1/producer/create_snapshot
```

Wait for the JSON response — it contains the path of the new snapshot file.
Keep this file; you will restore from it in step 5. (You can also use a
recent snapshot published by a trusted Telos infrastructure provider.)

### 2. Stop the Leap `nodeos` cleanly

```bash
sudo systemctl stop nodeos
# or, if you run it directly:
kill -SIGTERM "$(pgrep -x nodeos)"
```

Wait for the process to fully exit.

### 3. Update `config.ini`

Remove the `producer-threads` option if present — it was removed in the Spring
line and `nodeos` will **not start** if it is set:

```ini
# delete this line if you have it:
# producer-threads = 2
```

The rest of your Leap `config.ini` (peers, plugins, HTTP settings, etc.) is
carried over unchanged.

### 4. Replace the software

TelosZero Core conflicts with the `leap` package, so remove Leap first
(this does **not** touch your config or snapshot files), then install
TelosZero:

```bash
sudo apt-get remove -y leap

# download teloszero-core_1.2.2_amd64.deb from the release, then:
sudo apt-get update
sudo apt-get install -y ./teloszero-core_1.2.2_amd64.deb
```

Download link:
[TelosZero Core teloszero-v1.2.2](https://github.com/telosnetwork/teloszero-core/releases/tag/teloszero-v1.2.2).
Optionally verify the package:

```bash
sha256sum teloszero-core_1.2.2_amd64.deb
# expected:
# 285fdfc1abde5104892d94b1f380c6d79aba35eac3413f139119ea88574c5007
```

Verify the binary:

```bash
nodeos --full-version
```

### 5. Start TelosZero from the snapshot

Start `nodeos` **once** with `--snapshot`, pointing at a **fresh (empty)
state**. The simplest way is a clean data directory:

```bash
nodeos --snapshot /path/to/snapshot.bin --data-dir /path/to/new-datadir [your other options]
```

If instead you keep your existing data directory, delete the old Leap state
first (`rm -rf <data-dir>/state`) so the snapshot can load; nodeos will replay
irreversible blocks from `blocks.log` to catch up to head.

Remove the `--snapshot` flag on every subsequent restart:

```bash
sudo systemctl start nodeos
```

### 6. Confirm it is syncing

```bash
cleos get info
```

Watch that `head_block_num` advances and matches the network. Your node is now
running TelosZero Core.

## Rollback

Keep your Leap node's original data directory (or a Leap snapshot) untouched
until you are confident. To roll back:

```bash
sudo systemctl stop nodeos
sudo apt-get remove -y teloszero-core
sudo apt-get install -y ./leap_<version>_amd64.deb   # your previous package
sudo systemctl start nodeos                           # on the retained Leap data dir
```

## Notes

- **Migrate via snapshot, not in place.** Leap chain state is not loaded
  directly by the Spring line; the snapshot restore in step 5 is the supported
  path. This is the same procedure the EOS Network Foundation documented for
  the Leap → Spring upgrade.
- **`spring-util`.** Anything that called `leap-util` should call `spring-util`.
- **Clean shutdown matters.** Stop Leap cleanly (step 2) before taking the node
  out of service so the snapshot reflects a consistent state.

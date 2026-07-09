---
content_title: Upgrade a Non-Producing Node from Antelope Spring
---

## Who This Is For

This guide is for operators of **non-producing (non-BP) nodes** — exchanges,
API providers, wallet and explorer backends, and history/SHiP nodes — who are
running **Antelope Spring 1.2.x** and want to move to **TelosZero Core
`teloszero-v1.2.2`**.

TelosZero Core 1.2.2 is built from the Antelope Spring `v1.2.2` source and
**preserves Spring runtime compatibility**. For a non-BP node the upgrade is a
**drop-in binary swap**:

- Same on-disk data format (`blocks.log`, state, snapshots).
- Same `config.ini` options.
- Same command names: `nodeos`, `cleos`, `keosd`, `spring-util`.

That means **no replay, no resync, and no state migration**. You keep your
existing data directory and configuration.

## Before You Begin

- Confirm you are on Spring 1.2.x:

  ```bash
  nodeos --full-version
  ```

- Supported OS: Ubuntu 22.04 (Jammy) or Ubuntu 20.04 (Focal).
- Note your current `nodeos` data directory (`--data-dir`) and config
  directory (`--config-dir`). The upgrade does **not** change or move them.

## Upgrade Steps

### 1. (Recommended) Take a snapshot

A snapshot gives you a fast recovery point. While `nodeos` is running:

```bash
curl -X POST http://127.0.0.1:8888/v1/producer/create_snapshot
```

The response includes the snapshot file path. (This requires the
`producer_api_plugin` to be enabled. If you don't have it, a clean shutdown in
step 2 is sufficient for the swap; you can also fall back to your normal backup
process.)

### 2. Stop `nodeos` cleanly

Send a single `SIGINT`/`SIGTERM` and wait for the process to exit on its own so
state is flushed cleanly:

```bash
# systemd
sudo systemctl stop nodeos

# or, if you run it directly
kill -SIGTERM "$(pgrep -x nodeos)"
```

Wait for the process to fully exit. A clean shutdown lets the node restart from
existing state with no replay.

### 3. Remove the old Antelope Spring package

TelosZero Core ships the same binaries under a different package name
(`teloszero-core` vs `antelope-spring`), so remove the old package first.
This does **not** touch your data or config directories.

```bash
sudo apt-get remove -y antelope-spring
```

### 4. Install TelosZero Core

Download `teloszero-core_1.2.2_amd64.deb` from the
[TelosZero Core release](https://github.com/telosnetwork/teloszero-core/releases/tag/teloszero-v1.2.2)
and install it:

```bash
sudo apt-get update
sudo apt-get install -y ./teloszero-core_1.2.2_amd64.deb
```

Optionally verify the download before installing:

```bash
sha256sum teloszero-core_1.2.2_amd64.deb
# expected:
# 285fdfc1abde5104892d94b1f380c6d79aba35eac3413f139119ea88574c5007
```

### 5. Verify the binary

```bash
nodeos --full-version
```

### 6. Start `nodeos`

Start with your **existing** data directory and config — no extra flags, no
`--replay` or `--hard-replay` needed:

```bash
sudo systemctl start nodeos
```

### 7. Confirm it is syncing

```bash
cleos get info
```

Watch that `head_block_num` advances and matches the network. Your node is now
running TelosZero Core.

## Rollback

Because the data format is unchanged, rolling back is the reverse swap on the
same data directory:

```bash
sudo systemctl stop nodeos
sudo apt-get remove -y teloszero-core
sudo apt-get install -y ./antelope-spring_1.2.2_amd64.deb   # your previous package
sudo systemctl start nodeos
```

If anything looks wrong, restore the snapshot from step 1 with
`--snapshot <file>` on a fresh data directory.

## Notes

- **No protocol change.** This release does not alter consensus or state; it is
  a maintained repackaging of Spring 1.2.2. Nothing on-chain changes for your
  node.
- **Clean shutdown matters.** If `nodeos` was killed hard (e.g. `SIGKILL` or
  out-of-memory) before the swap, it may require a replay to rebuild state —
  the same behavior as Spring. A clean stop in step 2 avoids this.
- **`config.ini` is unchanged.** All Spring 1.2.x options are accepted as-is.

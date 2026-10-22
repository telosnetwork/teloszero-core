#!/usr/bin/env python3
"""Prepare and check an isolated, classical Telos Zero Quantum Lab bootstrap.

This stage creates a new chain identity. It does not add post-quantum signing.
"""

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import stat
import struct
import time
import urllib.request


MAINNET = "4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11"
TESTNET = "1eaa0824707c8c16bd25145493bf062aecddfeb56c736f6ba6397f3195f33c9f"
CORE_TAG = "teloszero-v1.2.2"
CORE_COMMIT = "fcb9f5fc627619a8b06cd6b871c462b1d70d3aa4"
PACKAGE_SHA256 = "285fdfc1abde5104892d94b1f380c6d79aba35eac3413f139119ea88574c5007"
CONFIG = {
    "max_block_net_usage": 1048576,
    "target_block_net_usage_pct": 1000,
    "max_transaction_net_usage": 524288,
    "base_per_transaction_net_usage": 12,
    "net_usage_leeway": 500,
    "context_free_discount_net_usage_num": 20,
    "context_free_discount_net_usage_den": 100,
    "max_block_cpu_usage": 5000000,
    "target_block_cpu_usage_pct": 1000,
    "max_transaction_cpu_usage": 150000,
    "min_transaction_cpu_usage": 100,
    "max_transaction_lifetime": 3600,
    "deferred_trx_expiration_window": 600,
    "max_transaction_delay": 3888000,
    "max_inline_action_size": 4096,
    "max_inline_action_depth": 4,
    "max_authority_depth": 6,
}
CONFIG_FIELDS = tuple(CONFIG)
BASE58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"


def fail(message):
    raise SystemExit(f"error: {message}")


def root_path(raw):
    path = Path(raw).expanduser().absolute()
    if path.is_symlink():
        fail("a Quantum Lab directory must not itself be a symlink")
    resolved = path.resolve(strict=False)
    if any("mainnet" in part.lower() or "testnet" in part.lower() for part in (*path.parts, *resolved.parts)):
        fail("use a dedicated non-symlink Quantum Lab path, away from existing networks")
    if path == Path("/"):
        fail("refusing filesystem root")
    return path


def write_new(path, data, mode=0o600):
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    fd = os.open(path, flags, mode)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as out:
            out.write(data)
    except Exception:
        path.unlink(missing_ok=True)
        raise


def verify_package(path):
    package = Path(path).expanduser().resolve(strict=True)
    if not package.is_file():
        fail("release package must be a regular file")
    digest = hashlib.sha256()
    with package.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    if digest.hexdigest() != PACKAGE_SHA256:
        fail("TelosZero Core package checksum does not match the pinned release")
    return digest.hexdigest()


def decode_k1_public_key(value):
    if value.startswith("EOS"):
        encoded, suffix = value[3:], b""
    elif value.startswith("PUB_K1_"):
        encoded, suffix = value[7:], b"K1"
    else:
        fail("genesis key must be a K1 public key")
    if not encoded or any(character not in BASE58 for character in encoded):
        fail("invalid K1 public-key encoding")
    number = 0
    for character in encoded:
        number = number * 58 + BASE58.index(character)
    leading_zeroes = len(encoded) - len(encoded.lstrip("1"))
    width = (number.bit_length() + 7) // 8
    payload = b"\x00" * leading_zeroes + number.to_bytes(width, "big")
    if len(payload) != 37:
        fail("K1 public key must decode to exactly 37 bytes")
    key, checksum = payload[:33], payload[33:]
    try:
        digest = hashlib.new("ripemd160", key + suffix).digest()
    except ValueError:
        digest = subprocess.run(
            ["openssl", "dgst", "-ripemd160", "-binary",
             "-provider", "default", "-provider", "legacy"],
            input=key + suffix, check=True, capture_output=True,
        ).stdout
    expected = digest[:4]
    if checksum != expected or key[0] not in (2, 3):
        fail("invalid K1 public-key checksum or point encoding")
    return key


def genesis_chain_id(genesis):
    """Match genesis_state::compute_chain_id() in tagged TelosZero Core."""
    when = dt.datetime.fromisoformat(genesis["initial_timestamp"])
    if when.tzinfo is not None:
        fail("genesis timestamp must be UTC without a timezone suffix")
    delta = when - dt.datetime(1970, 1, 1)
    microseconds = ((delta.days * 86400 + delta.seconds) * 1000000
                    + delta.microseconds)
    configuration = genesis["initial_configuration"]
    if not set(CONFIG_FIELDS).issubset(configuration):
        fail("required genesis configuration fields are missing")
    try:
        packed = (struct.pack("<Q", microseconds)
                  + b"\x00"  # K1 variant index
                  + decode_k1_public_key(genesis["initial_key"])
                  + struct.pack("<Q" + "I" * 14 + "H" * 2,
                                *(configuration[name] for name in CONFIG_FIELDS)))
    except (struct.error, OverflowError) as exc:
        fail(f"invalid genesis field: {exc}")
    return hashlib.sha256(packed).hexdigest()


def prepare(args):
    root = root_path(args.directory)
    if root.exists():
        fail("target already exists; preparation never overwrites a chain")
    package_sha256 = verify_package(args.package)
    decode_k1_public_key(args.public_key)
    try:
        timestamp = dt.datetime.strptime(args.timestamp, "%Y-%m-%dT%H:%M:%S.%f")
    except ValueError:
        fail("timestamp must be UTC as YYYY-MM-DDTHH:MM:SS.000")
    if timestamp.microsecond != 0 or timestamp.second != 0:
        fail("use a whole UTC minute for deterministic genesis")
    age = (dt.datetime.now(dt.timezone.utc).replace(tzinfo=None) - timestamp).total_seconds()
    if age < -60 or age > 86400:
        fail("genesis timestamp must be within the last 24 hours, not far in the future")

    genesis = {
        "initial_timestamp": args.timestamp,
        "initial_key": args.public_key,
        "initial_configuration": CONFIG,
    }
    expected_chain_id = genesis_chain_id(genesis)
    if expected_chain_id in {MAINNET, TESTNET}:
        fail("generated genesis matches an existing Telos Zero network")
    genesis_text = json.dumps(genesis, indent=2) + "\n"
    root.mkdir(mode=0o700, parents=False)
    (root / "config").mkdir(mode=0o700)
    (root / "data").mkdir(mode=0o700)
    write_new(root / "genesis.json", genesis_text)
    node_config = """# Private, isolated Quantum Lab bootstrap. Add a lab-only signer line here.
# signature-provider = PUB_K1_...=KEY:PVT_K1_...
# Never paste a mainnet/testnet or account owner key.
http-server-address = 127.0.0.1:8890
p2p-listen-endpoint = 127.0.0.1:9890
plugin = eosio::http_plugin
plugin = eosio::chain_plugin
plugin = eosio::chain_api_plugin
plugin = eosio::net_plugin
plugin = eosio::producer_plugin
producer-name = eosio
enable-stale-production = true
chain-state-db-size-mb = 1024
"""
    write_new(root / "config" / "config.ini", node_config)
    manifest = {
        "network": "Telos Zero Quantum Lab (private bootstrap)",
        "status": "prepared-not-started",
        "expected_core_tag": CORE_TAG,
        "expected_core_commit": CORE_COMMIT,
        "verified_package_sha256": package_sha256,
        "genesis_sha256": hashlib.sha256(genesis_text.encode()).hexdigest(),
        "expected_chain_id": expected_chain_id,
        "observed_chain_id": None,
        "http_api": "http://127.0.0.1:8890",
        "p2p": "127.0.0.1:9890",
        "post_quantum_signatures_enabled": False,
    }
    write_new(root / "manifest.json", json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared {root}; expected chain ID {expected_chain_id}.")
    print("Add a dedicated K1 signer to config/config.ini before starting nodeos.")


def read_manifest(root):
    marker = root / "manifest.json"
    if not marker.is_file():
        fail("Quantum Lab manifest missing")
    manifest = json.loads(marker.read_text(encoding="utf-8"))
    if manifest.get("network") != "Telos Zero Quantum Lab (private bootstrap)":
        fail("unexpected manifest network")
    genesis = (root / "genesis.json").read_bytes()
    if hashlib.sha256(genesis).hexdigest() != manifest.get("genesis_sha256"):
        fail("genesis changed after preparation")
    if genesis_chain_id(json.loads(genesis)) != manifest.get("expected_chain_id"):
        fail("manifest chain ID does not match genesis")
    return manifest


def validate_private_config(root):
    config = root / "config" / "config.ini"
    if not config.is_file() or config.is_symlink():
        fail("private nodeos config is missing or is a symlink")
    if stat.S_IMODE(config.stat().st_mode) & 0o077:
        fail("private nodeos config must not be readable by group or others")
    settings = {}
    for raw in config.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            fail("invalid private nodeos config line")
        name, value = (part.strip() for part in line.split("=", 1))
        settings.setdefault(name, []).append(value)
    expected = {
        "http-server-address": ["127.0.0.1:8890"],
        "p2p-listen-endpoint": ["127.0.0.1:9890"],
        "plugin": ["eosio::http_plugin", "eosio::chain_plugin",
                   "eosio::chain_api_plugin", "eosio::net_plugin",
                   "eosio::producer_plugin"],
        "producer-name": ["eosio"],
        "enable-stale-production": ["true"],
        "chain-state-db-size-mb": ["1024"],
    }
    if set(settings) != set(expected) | {"signature-provider"}:
        fail("private config contains missing or unapproved options")
    for name, values in expected.items():
        if settings.get(name) != values:
            fail(f"private config option {name} differs from prepared isolation")
    signer = settings["signature-provider"]
    if len(signer) != 1 or "=KEY:" not in signer[0]:
        fail("exactly one KEY signature provider is required")
    public, private = signer[0].split("=KEY:", 1)
    if not private or "<" in private or ">" in private:
        fail("a real lab-only signing key is required")
    genesis = json.loads((root / "genesis.json").read_text(encoding="utf-8"))
    if decode_k1_public_key(public) != decode_k1_public_key(genesis["initial_key"]):
        fail("signer public key does not match genesis key")


def start(args):
    root = root_path(args.directory)
    manifest = read_manifest(root)
    validate_private_config(root)
    nodeos = shutil.which(args.nodeos)
    if not nodeos:
        fail("nodeos binary not found")
    data = root / "data"
    if not data.is_dir() or data.is_symlink():
        fail("dedicated chain data directory is missing or is a symlink")
    populated = any(data.iterdir())
    if populated and not manifest["observed_chain_id"]:
        fail("unverified chain data exists; inspect it before a restart")
    if not populated and manifest["observed_chain_id"]:
        fail("pinned chain data is missing; refuse silent reset")
    command = [nodeos, "--config-dir", str(root / "config"),
               "--data-dir", str(data)]
    if not populated:
        command += ["--genesis-json", str(root / "genesis.json")]
    os.execv(nodeos, command)


def get_info():
    request = urllib.request.Request(
        "http://127.0.0.1:8890/v1/chain/get_info",
        data=b"{}",
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=5) as response:
        if response.status != 200:
            fail(f"API returned HTTP {response.status}")
        return json.load(response)


def checked_info():
    info = get_info()
    chain_id = info.get("chain_id", "")
    if not re.fullmatch(r"[0-9a-f]{64}", chain_id):
        fail("invalid chain ID from API")
    if chain_id in {MAINNET, TESTNET}:
        fail("API is serving a real Telos network; stop and investigate")
    return info


def pin(args):
    root = root_path(args.directory)
    manifest = read_manifest(root)
    if manifest["observed_chain_id"] is not None:
        fail("chain ID already pinned; never repin an existing network")
    info = checked_info()
    if info["chain_id"] != manifest["expected_chain_id"]:
        fail("API chain ID does not match the prepared genesis")
    manifest["observed_chain_id"] = info["chain_id"]
    manifest["status"] = "private-chain-observed"
    replacement = root / "manifest.json.new"
    write_new(replacement, json.dumps(manifest, indent=2) + "\n")
    os.replace(replacement, root / "manifest.json")
    print(f"Pinned lab chain ID: {info['chain_id']}")


def check(args):
    root = root_path(args.directory)
    manifest = read_manifest(root)
    if not manifest["observed_chain_id"]:
        fail("pin the new chain ID before checking")
    first = checked_info()
    if first["chain_id"] != manifest["expected_chain_id"]:
        fail("API chain ID does not match pinned Quantum Lab identity")
    for _ in range(15):
        time.sleep(2)
        second = checked_info()
        if second["chain_id"] != manifest["expected_chain_id"]:
            fail("API chain ID changed")
        head = int(second.get("head_block_num", 0))
        lib = int(second.get("last_irreversible_block_num", 0))
        if head > int(first.get("head_block_num", 0)) and lib > int(first.get("last_irreversible_block_num", 0)):
            break
    else:
        fail("head or irreversible block did not advance within 30 seconds")
    print(f"Quantum Lab identity OK; head {head}; LIB {lib}. PQ signatures are not enabled.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prep = commands.add_parser("prepare", help="create a new isolated genesis and config")
    prep.add_argument("directory")
    prep.add_argument("--package", required=True, help="verified teloszero-core_1.2.2_amd64.deb")
    prep.add_argument("--public-key", required=True)
    prep.add_argument("--timestamp", required=True)
    prep.set_defaults(func=prepare)
    for name, action in (("pin", pin), ("check", check)):
        command = commands.add_parser(name)
        command.add_argument("directory")
        command.set_defaults(func=action)
    launch = commands.add_parser("start", help="run the isolated nodeos process")
    launch.add_argument("directory")
    launch.add_argument("--nodeos", default="nodeos", help="path to installed TelosZero nodeos")
    launch.set_defaults(func=start)
    args = parser.parse_args()
    try:
        args.func(args)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        fail(str(exc))


if __name__ == "__main__":
    main()

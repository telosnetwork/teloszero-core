#!/usr/bin/env python3
"""Exercise a normal Savanna policy transition with two release binaries."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="exact candidate source checkout")
    parser.add_argument("--baseline-nodeos", required=True, type=Path)
    parser.add_argument("--baseline-version", default="v1.4.0-f36f91f3898e9e742b888d086fce92d6a1288517")
    parser.add_argument("--candidate-bin", required=True, type=Path, help="directory containing packaged executables")
    parser.add_argument("--candidate-version", required=True)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--scratch-dir", type=Path, help="parent directory for temporary fixture state")
    parser.add_argument("--timeout", type=int, default=270, help="maximum test duration in seconds")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    for name in ("source", "baseline_nodeos", "candidate_bin", "evidence", "scratch_dir"):
        if (path := getattr(args, name)) is not None:
            setattr(args, name, path.resolve())
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    return args


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify_binary_version(path, expected):
    require(path.is_file() and os.access(path, os.X_OK), f"missing executable: {path}")
    result = subprocess.run([path, "--full-version"], check=True, capture_output=True, text=True, timeout=15)
    actual = result.stdout.strip()
    require(actual == expected, f"{path.name} version mismatch: expected {expected}, got {actual}")
    return actual


def rpc(node, command, payload=None):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    request = urllib.request.Request(
        f"http://127.0.0.1:{node.port}/v1/chain/{command}",
        data=json.dumps(payload or {}).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with opener.open(request, timeout=3) as response:
        return json.load(response)


def wait_until(description, predicate, seconds=60):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            value = predicate()
        except (TimeoutError, urllib.error.URLError):
            value = None
        if value:
            return value
        time.sleep(0.15)
    raise TimeoutError(f"timed out waiting for {description}")


def policy_summary(policy):
    if not isinstance(policy, dict) or "generation" not in policy:
        return None
    return {
        "generation": int(policy["generation"]),
        "threshold": int(policy["threshold"]),
        "finalizers": [
            {
                "description": finalizer["description"],
                "weight": int(finalizer["weight"]),
                "public_key": finalizer["public_key"],
            }
            for finalizer in policy["finalizers"]
        ],
    }


def assert_loopback_ports_available():
    for port in (8788, 8888, 8889, 9776, 9876, 9877):
        with socket.socket() as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("127.0.0.1", port))


def run_cluster(args):
    # Import from the copied source harness after switching to disposable storage.
    sys.path.insert(0, str(Path.cwd() / "tests"))
    from TestHarness import Cluster, TestHelper, WalletMgr

    assert_loopback_ports_available()
    cl = Cluster(host="127.0.0.1", keepLogs=True)
    wallet = WalletMgr(True, host="127.0.0.1")
    succeeded = False
    try:
        cl.setWalletMgr(wallet)
        ports = {-100: (8788, 9776), 0: (8888, 9876), 1: (8889, 9877)}
        loopback = {
            index: f"--http-server-address 127.0.0.1:{http} --p2p-listen-endpoint 127.0.0.1:{p2p}"
            for index, (http, p2p) in ports.items()
        }
        require(
            cl.launch(
                pnodes=2,
                totalNodes=2,
                prodCount=1,
                totalProducers=2,
                delay=1,
                onlySetProds=True,
                loadSystemContract=False,
                activateIF=True,
                biosFinalizer=False,
                specificNodeosInstances={1: str(args.baseline_nodeos)},
                specificExtraNodeosArgs=loopback,
                extraNodeosArgs=(
                    " --chain-state-db-size-mb 256 --chain-state-db-guard-size-mb 16"
                    " --wasm-runtime eos-vm"
                ),
            ),
            "cluster bootstrap failed",
        )
        nodes = {"candidate": cl.getNode(0), "baseline": cl.getNode(1)}
        expected_versions = {"candidate": args.candidate_version, "baseline": args.baseline_version}

        starting_info = {label: rpc(node, "get_info") for label, node in nodes.items()}
        for label, info in starting_info.items():
            require(info["server_full_version_string"] == expected_versions[label], f"{label} RPC version mismatch")
        chain_ids = {info["chain_id"] for info in starting_info.values()}
        require(len(chain_ids) == 1, "mixed-version nodes did not join the same chain")

        def current_policy(node):
            return policy_summary(rpc(node, "get_finalizer_info")["active_finalizer_policy"])

        initial = wait_until(
            "both nodes to report the same initial active finalizer policy",
            lambda: (first if (first := current_policy(nodes["candidate"])) is not None
                     and first == current_policy(nodes["baseline"]) else None),
        )
        expected_generations = (initial["generation"], initial["generation"] + 1)
        require(len(initial["finalizers"]) == 2, "expected the two producing nodes as the initial finalizers")
        print(f"Before transition both versions report active finalizer generation {expected_generations[0]}", flush=True)
        prior_lib = min(int(info["last_irreversible_block_num"]) for info in starting_info.values())

        # Change the on-chain policy descriptions while retaining the same two
        # authorized finalizers. This gives both versions overlapping active
        # and pending policies without relying on unavailable signatures.
        descriptions = ["candidate next policy", "baseline next policy"]
        transaction_id = cl.setFinalizers(list(nodes.values()), nodes["candidate"], finalizerNames=descriptions)
        require(transaction_id, "the ordinary finalizer policy update was not accepted")

        initial_keys = {fin["public_key"] for fin in initial["finalizers"]}
        pending_seen = {}

        def inspect_transition():
            active = {}
            for label, node in nodes.items():
                info = rpc(node, "get_finalizer_info")
                pending = policy_summary(info["pending_finalizer_policy"])
                if pending and pending["generation"] == expected_generations[1]:
                    require({fin["public_key"] for fin in pending["finalizers"]} == initial_keys,
                            f"{label} pending policy lost the overlapping finalizers")
                    pending_seen.setdefault(label, pending)
                active[label] = policy_summary(info["active_finalizer_policy"])
            first = active["candidate"]
            return first if first and first == active["baseline"] and first["generation"] == expected_generations[1] else None

        transitioned = wait_until("both versions to activate the updated finalizer policy", inspect_transition, seconds=75)
        require(set(pending_seen) == set(nodes), "both versions must report the new pending policy during transition")
        require(all(pending == transitioned for pending in pending_seen.values()),
                "reported pending policies differ from the new common active policy")
        require({fin["description"] for fin in transitioned["finalizers"]} == set(descriptions),
                "the common active policy does not contain the submitted update")

        activated_heads = {label: int(rpc(node, "get_info")["head_block_num"]) for label, node in nodes.items()}
        target = max(prior_lib, *activated_heads.values()) + 6

        def synced_lib():
            libs = {label: int(rpc(node, "get_info")["last_irreversible_block_num"]) for label, node in nodes.items()}
            return libs if min(libs.values()) >= target else None

        libs = wait_until(f"both versions to finalize block {target}", synced_lib)
        block_ids = {label: rpc(node, "get_block", {"block_num_or_id": target})["id"] for label, node in nodes.items()}
        block_id = block_ids["candidate"]
        require(re.fullmatch(r"[0-9a-f]{64}", block_id) is not None, "unexpected finalized block ID format")
        require(block_ids["baseline"] == block_id, "the two versions finalized different IDs at the same height")
        evidence = {
            "versions": expected_versions,
            "chain_id": chain_ids.pop(),
            "initial_active_policy": initial,
            "observed_pending_policy": pending_seen,
            "active_policy_after_update": transitioned,
            "policy_update_transaction_id": transaction_id,
            "post_transition_finalized_height": target,
            "post_transition_finalized_id_by_node": block_ids,
            "last_irreversible_height_by_node": libs,
        }
        args.evidence.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"Both versions finalized block {target} ({block_id}) after activating policy {expected_generations[1]}", flush=True)
        succeeded = True
    finally:
        try:
            TestHelper.shutdown(cl, wallet, testSuccessful=succeeded)
        finally:
            wallet.shutdown()


def prepare_workspace(workdir, args):
    for name in ("nodeos", "cleos", "keosd", "spring-util"):
        executable = args.candidate_bin / name
        require(executable.is_file() and os.access(executable, os.X_OK), f"missing executable: {executable}")
        (workdir / "bin").mkdir(exist_ok=True)
        (workdir / "bin" / name).symlink_to(executable)
    shutil.copytree(args.source / "tests/TestHarness", workdir / "tests/TestHarness")
    core_symbol = workdir / "tests/TestHarness/core_symbol.py"
    core_symbol.write_text("CORE_SYMBOL = 'SYS'\n")
    for path in ("unittests/contracts/eosio.boot", "libraries/testing/contracts/eosio.bios"):
        shutil.copytree(args.source / path, workdir / path)


def supervise(args):
    verify_binary_version(args.baseline_nodeos, args.baseline_version)
    verify_binary_version(args.candidate_bin / "nodeos", args.candidate_version)
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    require(not args.evidence.exists(), f"evidence path already exists: {args.evidence}")
    with tempfile.TemporaryDirectory(prefix="mixed-finality-", dir=args.scratch_dir) as workspace:
        workdir = Path(workspace)
        prepare_workspace(workdir, args)
        command = [
            sys.executable, str(Path(__file__).resolve()), "--worker",
            "--source", str(args.source),
            "--baseline-nodeos", str(args.baseline_nodeos), "--baseline-version", args.baseline_version,
            "--candidate-bin", str(args.candidate_bin), "--candidate-version", args.candidate_version,
            "--evidence", str(args.evidence),
        ]
        env = dict(os.environ, NO_PROXY="localhost,127.0.0.1", no_proxy="localhost,127.0.0.1")
        proc = subprocess.Popen(command, cwd=workdir, env=env, start_new_session=True)
        try:
            result = proc.wait(timeout=args.timeout)
            require(result == 0 and args.evidence.is_file(), f"mixed finality worker did not pass (exit {result})")
        finally:
            # The worker and only its own fixture processes share this new group.
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
            # A child may outlive the worker, for example on early bootstrap failure.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


if __name__ == "__main__":
    options = arguments()
    if options.worker:
        run_cluster(options)
    else:
        def interrupted(signum, _frame):
            raise SystemExit(128 + signum)

        signal.signal(signal.SIGTERM, interrupted)
        supervise(options)

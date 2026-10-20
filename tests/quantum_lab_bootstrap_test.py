"""Regression tests for the private Quantum Lab chain identity utility."""

import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


REPO = Path(os.environ.get("TELOSZERO_SOURCE", Path(__file__).resolve().parents[1]))
SCRIPT = Path(os.environ.get("QUANTUM_LAB_SCRIPT", REPO / "scripts/quantum-lab/bootstrap.py"))
spec = importlib.util.spec_from_file_location("quantum_lab_bootstrap", SCRIPT)
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)


class QuantumLabBootstrapTests(unittest.TestCase):
    def genesis(self, name):
        path = REPO / "genesis" / f"telos_{name}_genesis.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def test_canonical_telos_chain_ids(self):
        self.assertEqual(lab.genesis_chain_id(self.genesis("mainnet")), lab.MAINNET)
        self.assertEqual(lab.genesis_chain_id(self.genesis("testnet")), lab.TESTNET)

    def test_bad_public_key_checksum_is_rejected(self):
        valid = self.genesis("testnet")["initial_key"]
        bad = valid[:-1] + ("1" if valid[-1] != "1" else "2")
        with self.assertRaises(SystemExit):
            lab.decode_k1_public_key(bad)

    def test_new_identity_and_wrong_api_rejection(self):
        with tempfile.TemporaryDirectory(prefix="quantum-lab-check-") as temporary:
            root = Path(temporary) / "lab"
            timestamp = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:00.000")
            key = self.genesis("testnet")["initial_key"]
            lab.prepare(SimpleNamespace(directory=str(root), public_key=key, timestamp=timestamp))
            manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
            expected = manifest["expected_chain_id"]
            self.assertNotIn(expected, (lab.MAINNET, lab.TESTNET))
            self.assertIsNone(manifest["observed_chain_id"])
            with self.assertRaises(SystemExit):
                lab.prepare(SimpleNamespace(directory=str(root), public_key=key, timestamp=timestamp))
            with mock.patch.object(lab, "checked_info", return_value={"chain_id": "a" * 64}):
                with self.assertRaises(SystemExit):
                    lab.pin(SimpleNamespace(directory=str(root)))
            self.assertIsNone(json.loads((root / "manifest.json").read_text())["observed_chain_id"])
            with mock.patch.object(lab, "checked_info", return_value={"chain_id": expected}):
                lab.pin(SimpleNamespace(directory=str(root)))
            samples = [
                {"chain_id": expected, "head_block_num": 1, "last_irreversible_block_num": 1},
                {"chain_id": expected, "head_block_num": 5, "last_irreversible_block_num": 3},
            ]
            with mock.patch.object(lab, "checked_info", side_effect=samples), \
                 mock.patch.object(lab.time, "sleep"):
                lab.check(SimpleNamespace(directory=str(root)))


if __name__ == "__main__":
    unittest.main()

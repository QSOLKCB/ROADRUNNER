#!/usr/bin/env python3
"""Harness contract tests; accepts normal binary and optionally frozen-clock binary."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from plan_run import compare, summarize, read_flags

BINARIES = sys.argv[1:]
sys.argv[1:] = []


class PlanBenchTest(unittest.TestCase):
    def test_harness(self):
        self.assertTrue(BINARIES, "pass a benchmark binary")
        for index, binary in enumerate(BINARIES):
            for family in ("local", "weighted", "zero", "unit"):
                rows = {}
                for mode in ("legacy", "plan"):
                    rows[mode] = json.loads(subprocess.check_output(
                        [binary, mode, family, "4", "10", "123"], text=True))
                    row = rows[mode]
                    self.assertEqual(row["allocations"], row["frees"])
                    self.assertEqual(len(row["samples"]), 10)
                    self.assertGreaterEqual(row["constructor_peak_requested_bytes"], row["steady_requested_bytes"])
                    self.assertEqual(row["query_allocations"], 0)
                result = compare(rows["legacy"], rows["plan"])
                self.assertEqual(rows["plan"]["steady_requested_bytes"] - rows["legacy"]["steady_requested_bytes"],
                                 rows["plan"]["plan_bytes"] - 16 * 8)
                if index:
                    self.assertTrue(all(s["us"] == 0 for s in rows["plan"]["samples"]))
                    self.assertIsNone(result["lifecycle_plan_over_legacy"])
                    self.assertEqual(result["status"], "inconclusive_clock_resolution")
                broken = copy.deepcopy(rows["plan"])
                broken["samples"][0]["cost"] += 1
                with self.assertRaises(ValueError):
                    compare(rows["legacy"], broken)

    def test_seed_boundaries(self):
        for binary in BINARIES:
            for seed in ("1000000009", "1000000010", "4294967295"):
                row = json.loads(subprocess.check_output(
                    [binary, "legacy", "local", "4", "1", seed], text=True))
                self.assertEqual(row["seed"], int(seed))
            for seed in ("0", "4294967296", "9999999999999999999999", "-1", "+1"):
                self.assertEqual(subprocess.run(
                    [binary, "legacy", "local", "4", "1", seed], capture_output=True).returncode, 2)

    def test_compilation_database_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "compile_commands.json"
            contents = '[{"command": "cc -O3 -DNDEBUG -std=c17"}]'
            database.write_text(contents)
            self.assertEqual(read_flags("@" + str(database)), contents)
            self.assertEqual(read_flags("-O2"), "-O2")

    def test_missing_compilation_database(self):
        script = Path(__file__).resolve().parents[1] / "benchmarks" / "plan_run.py"
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "result.json"
            result = subprocess.run([sys.executable, str(script), "--binary", BINARIES[0],
                                     "--output", str(output), "--flags=@" + str(Path(directory) / "missing")],
                                    capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def test_partial_repetitions(self):
        pair = {"split": "declared", "family": "weighted", "queries": 100, "repeat": 0,
                "comparison": {"status": "measured", "lifecycle_plan_over_legacy": .5,
                               "query_saving_us": 1}}
        for pairs in ([pair], [pair, copy.deepcopy(pair)]):
            result = summarize(pairs, 2)
            self.assertTrue(all(r["first_demonstrated_Q"] is None for r in result))
            row = next(r for r in result if r["family"] == "weighted" and r["split"] == "declared")
            self.assertEqual(row["discrete_Q_results"][0]["status"], "incomplete_repetitions")
        second = copy.deepcopy(pair)
        second["repeat"] = 1
        result = summarize([pair, second], 2)
        row = next(r for r in result if r["family"] == "weighted" and r["split"] == "declared")
        self.assertEqual(row["first_demonstrated_Q"], 100)

    def test_no_false_crossing(self):
        pairs = [{"split": "declared", "family": "weighted", "queries": 100,
                  "repeat": 0, "comparison": {"status": "inconclusive_clock_resolution",
                                 "lifecycle_plan_over_legacy": None, "query_saving_us": None}}]
        result = summarize(pairs, 2)
        self.assertTrue(all(r["first_demonstrated_Q"] is None for r in result))


if __name__ == "__main__":
    unittest.main()

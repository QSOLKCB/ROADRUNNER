#!/usr/bin/env python3
"""Harness contract tests; accepts normal binary and optionally frozen-clock binary."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from plan_run import compare, summarize

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

    def test_no_false_crossing(self):
        pairs = [{"split": "declared", "family": "weighted", "queries": 100,
                  "comparison": {"status": "inconclusive_clock_resolution",
                                 "lifecycle_plan_over_legacy": None, "query_saving_us": None}}]
        result = summarize(pairs)
        self.assertTrue(all(r["first_demonstrated_Q"] is None for r in result))


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Fixed repeated-target plan experiment. Retains all pairs, including zero timings."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import time

from run import command, source_digest

# Declared before timing: no seed/size search. Second size/seed is held out.
WORKLOADS = [("declared", 16, 123456789), ("heldout", 24, 987654321)]
FAMILIES = ("local", "weighted", "zero", "unit")
COUNTS = (1, 10, 100, 1000)


def compare(legacy, plan):
    for key in ("family", "side", "queries", "seed", "target", "graph_hash", "query_hash"):
        if legacy[key] != plan[key]:
            raise ValueError(f"unmatched {key}")
    if len(legacy["samples"]) != len(plan["samples"]):
        raise ValueError("unmatched sample count")
    for left, right in zip(legacy["samples"], plan["samples"]):
        for key in ("source", "status", "cost", "path_length", "expanded", "relaxed", "pushes", "path_hash"):
            if left[key] != right[key]:
                raise ValueError(f"query parity failure: {key}")
    for row in (legacy, plan):
        if not row["verified"] or row["query_allocations"] or row["live_bytes_after_teardown"]:
            raise ValueError("verification/allocation failure")
    a, b = legacy["lifecycle_us"], plan["lifecycle_us"]
    am = statistics.median(s["us"] for s in legacy["samples"])
    bm = statistics.median(s["us"] for s in plan["samples"])
    measurable = a > 0 and b > 0 and am > 0 and bm > 0
    return {"lifecycle_plan_over_legacy": b / a if measurable else None,
            "query_median_plan_over_legacy": bm / am if measurable else None,
            "query_saving_us": am - bm if measurable else None,
            "status": "measured" if measurable else "inconclusive_clock_resolution"}


def summarize(pairs):
    summaries = []
    for split, side, seed in WORKLOADS:
        for family in FAMILIES:
            rows = []
            for q in COUNTS:
                subset = [p for p in pairs if (p["split"], p["family"], p["queries"]) == (split, family, q)]
                if not subset:
                    continue
                valid = all(p["comparison"]["status"] == "measured" for p in subset)
                ratios = [p["comparison"]["lifecycle_plan_over_legacy"] for p in subset]
                savings = [p["comparison"]["query_saving_us"] for p in subset]
                rows.append({"queries": q, "pair_ratios": ratios,
                             "median_lifecycle_ratio": statistics.median(ratios) if valid else None,
                             "all_pairs_improve": valid and all(r < 1 for r in ratios),
                             "all_pairs_improve_10_percent": valid and all(r <= .9 for r in ratios),
                             "positive_query_savings": valid and all(s > 0 for s in savings),
                             "status": "measured" if valid else "inconclusive_clock_resolution"})
            crossings = [r["queries"] for r in rows if r["all_pairs_improve"] and r["positive_query_savings"]]
            summaries.append({"split": split, "family": family, "side": side, "seed": seed,
                              "discrete_Q_results": rows,
                              "first_demonstrated_Q": min(crossings) if crossings else None,
                              "interpretation": "Observed discrete-Q lifecycle crossing only; no universal guarantee."
                              if crossings else "No demonstrated finite break-even in the tested Q set."})
    return summaries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/rr_plan_bench")
    parser.add_argument("--output", required=True)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--compiler", default="cc")
    parser.add_argument("--flags", required=True)
    args = parser.parse_args()
    if not 2 <= args.repeats <= 9:
        parser.error("repeats must be 2..9 for alternating pairs")
    binary = Path(args.binary).resolve()
    metadata = {"utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "source_commit": command(["git", "rev-parse", "HEAD"]),
                "source_status": command(["git", "status", "--porcelain"]),
                "source_digest": source_digest(), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                "compiler": command([args.compiler, "--version"]), "flags": args.flags,
                "platform": platform.platform(), "uname": list(platform.uname()),
                "cpu": command(["sysctl", "-n", "machdep.cpu.brand_string"])
                if platform.system() == "Darwin" else command(["lscpu"]),
                "workloads": WORKLOADS, "families": FAMILIES, "query_counts": COUNTS,
                "timing_scope": "CLOCK_MONOTONIC; no warmups. Query includes search/reconstruction; legacy certifies per call. Common heuristic generated once. Plan setup includes copy, certification and caller-h release. Lifecycle is one contiguous interval containing input generation, graph/workspace/heuristic setup, plan setup, queries and core/heuristic teardown. Common harness buffer allocation/free, independent verification and serialization occur outside that interval; their timing is separately reported. All paths are retained in common harness buffers for later verification. Process wall includes the entire process.",
                "memory_scope": "Requested bytes of all harness and core allocations, including caller h/copy constructor peak, oracle, inputs and samples; excludes stack, allocator metadata, libc internals and RSS. Plan frees caller h after creation; legacy retains it.",
                "comparison_warning": "Shared-host characterization only. Hashes are replay diagnostics; each C run independently validates exact shortest costs and route edges. Held-out cases fixed before measurements."}
    pairs, references = [], []
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    def save():
        output.write_text(json.dumps({"metadata": metadata, "pairs": pairs, "auto_references": references,
                                      "summary": summarize(pairs)}, indent=2) + "\n")

    def run(mode, family, side, q, seed):
        start = time.perf_counter()
        result = subprocess.run([str(binary), mode, family, str(side), str(q), str(seed)],
                                check=True, capture_output=True, text=True, timeout=90)
        row = json.loads(result.stdout)
        row["process_end_to_end_seconds"] = time.perf_counter() - start
        return row

    for repeat in range(args.repeats):
        for split, side, seed in WORKLOADS:
            for family in FAMILIES:
                for q in COUNTS:
                    order = ("legacy", "plan") if repeat % 2 == 0 else ("plan", "legacy")
                    rows = {mode: run(mode, family, side, q, seed) for mode in order}
                    pairs.append({"repeat": repeat, "split": split, "family": family, "queries": q,
                                  "order": order, **rows, "comparison": compare(rows["legacy"], rows["plan"])})
                    save()  # Preserve every completed pair even if a later process fails.
                    if family == "unit":
                        ref = run("auto", family, side, q, seed)
                        if [(s["source"], s["status"], s["cost"]) for s in ref["samples"]] != [
                                (s["source"], s["status"], s["cost"]) for s in rows["legacy"]["samples"]]:
                            raise ValueError("AUTO reference cost mismatch")
                        references.append({"repeat": repeat, "split": split, **ref})
                        save()
    print(f"Saved {len(pairs)} alternating process pairs and {len(references)} AUTO references to {output}")


if __name__ == "__main__":
    main()

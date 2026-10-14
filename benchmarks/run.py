#!/usr/bin/env python3
"""Bounded process-isolated benchmark evidence; no speed threshold on shared hosts."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
def command(args):
    try:
        return subprocess.check_output(args, cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        return str(error)
def source_digest():
    paths = [ROOT / "CMakeLists.txt", ROOT / "Makefile"]
    for directory in ("include", "src", "tests", "benchmarks"):
        paths.extend(p for p in (ROOT / directory).rglob("*") if p.is_file() and "__pycache__" not in str(p))
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(str(path.relative_to(ROOT)).encode()+b"\0"+path.read_bytes()+b"\0")
    return digest.hexdigest()
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/rr_bench")
    parser.add_argument("--output", required=True)
    parser.add_argument("--side", type=int, default=64)
    parser.add_argument("--queries", type=int, default=201)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--seed", type=int, default=123456789)
    parser.add_argument("--flags", required=True, help="Actual compile/link flags or compilation database path")
    parser.add_argument("--compiler", default="cc")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 9:
        parser.error("repeats must be 1..9")
    binary = Path(args.binary).resolve()
    cpu = command(["sysctl", "-n", "machdep.cpu.brand_string"]) if platform.system() == "Darwin" else command(["lscpu"])
    metadata = {"utc": datetime.datetime.now(datetime.timezone.utc).isoformat(), "source_commit":command(["git","rev-parse","HEAD"]),
        "source_status": command(["git","status","--porcelain"]), "source_digest":source_digest(),
        "binary_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(), "compiler":command([args.compiler,"--version"]),
        "flags":Path(args.flags[1:]).read_text() if args.flags.startswith("@") else args.flags, "platform":platform.platform(), "uname":list(platform.uname()), "cpu":cpu,
        "cpuinfo_fallback": Path("/proc/cpuinfo").read_text()[:20000] if platform.system()=="Linux" and Path("/proc/cpuinfo").exists() else None,
        "runner_arch":os.environ.get("RUNNER_ARCH"), "github_sha":os.environ.get("GITHUB_SHA"),
        "clock":"CLOCK_MONOTONIC", "quantiles":"median sorted[n//2]; p95/p99 nearest-rank ceil(p*n)-1",
        "timing_scope":"query includes heuristic generation/certification and path reconstruction; excludes path verifier; first is fresh workspace, not evicted CPU cache",
        "memory_scope":"tracked requested bytes including graph, workspace and harness inputs/verification/samples; excludes allocator metadata and OS RSS",
        "comparison_warning":"Shared-host characterization only; compare matched graph/query hashes and source/hardware/compiler/flags"}
    cases = [("grid","auto","row"),("grid","auto","morton"),("grid","dijkstra","row"),("grid","astar","row"),
        ("weighted","dijkstra","row"),("weighted","dijkstra","morton"),("weighted","astar","row"),
        ("local","auto","row"),("local","dijkstra","row"),("sparse","dijkstra","row"),
        ("disconnected","auto","row"),("corridor","auto","row")]
    results=[]
    for repeat in range(args.repeats):
        # Reverse order on alternating repeats to reduce systematic ordering bias.
        for family, strategy, layout in (cases if repeat % 2 == 0 else reversed(cases)):
            start=time.perf_counter()
            process=subprocess.run([str(binary),family,strategy,layout,str(args.side),str(args.queries),str(args.seed)],check=True,capture_output=True,text=True,timeout=90)
            row=json.loads(process.stdout); row["process_end_to_end_seconds"]=time.perf_counter()-start; row["repeat"]=repeat; results.append(row)
    for family in ("grid","weighted"):
        for repeat in range(args.repeats):
            pair=[r for r in results if r["family"]==family and r["repeat"]==repeat and r["strategy"]==("auto" if family=="grid" else "dijkstra")]
            assert len(pair)==2
            assert len({(r["graph_hash"],r["query_hash"],r["cost_checksum"]) for r in pair})==1
    output=Path(args.output); output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps({"metadata":metadata,"results":results},indent=2)+"\n")
    summary=output.with_suffix(".txt")
    summary.write_text("ROADRUNNER shared-host characterization (microseconds; no speed guarantee)\n"+
        "source "+metadata["source_commit"]+"\ndigest "+metadata["source_digest"]+"\n"+
        "\n".join(f"{r['repeat']} {r['family']:12s} {r['strategy']:8s} {r['layout']:6s} median={r['median_us']:.3f} p95={r['p95_us']:.3f} p99={r['p99_us']:.3f} setup={r['setup_us']:.3f} expanded={r['expanded']}" for r in results)+"\n")
    print(summary.read_text())
if __name__ == "__main__":
    main()

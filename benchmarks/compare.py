#!/usr/bin/env python3
"""Alternating paired native binaries. Does not infer statistical certainty."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import time
import datetime

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--baseline",required=True)
    parser.add_argument("--candidate",required=True)
    parser.add_argument("--baseline-commit",required=True)
    parser.add_argument("--candidate-commit",required=True)
    parser.add_argument("--baseline-core",required=True)
    parser.add_argument("--candidate-core",required=True)
    parser.add_argument("--output",required=True)
    parser.add_argument("--flags",required=True)
    parser.add_argument("--compiler",default="cc")
    parser.add_argument("--seed",type=int,default=123456789)
    parser.add_argument("--side",type=int,default=64)
    parser.add_argument("--queries",type=int,default=201)
    parser.add_argument("--pairs",type=int,default=5)
    parser.add_argument("--allow-bfs-work-change",action="store_true")
    args=parser.parse_args()
    if not 3<=args.pairs<=9:
        parser.error("pairs must be 3..9")
    cases=[("grid","auto","row"),("grid","dijkstra","row"),("grid","astar","row"),
        ("weighted","dijkstra","row"),("weighted","astar","row"),("local","auto","row"),
        ("local","dijkstra","row"),("sparse","dijkstra","row"),("disconnected","auto","row"),("corridor","auto","row")]
    metadata={"utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),"platform":platform.platform(),"uname":list(platform.uname()),
        "cpuinfo":Path("/proc/cpuinfo").read_text()[:20000] if Path("/proc/cpuinfo").exists() else None,
        "compiler":subprocess.check_output([args.compiler,"--version"],text=True),"flags":args.flags,
        "baseline_commit":args.baseline_commit,"candidate_commit":args.candidate_commit,
        "baseline_binary_sha256":digest(args.baseline),"candidate_binary_sha256":digest(args.candidate),
        "baseline_core_sha256":digest(args.baseline_core),"candidate_core_sha256":digest(args.candidate_core),
        "method":"AB/BA alternate by pair, same logical graph/query/strategy; ratios candidate/baseline; shared host, descriptive only"}
    pairs=[]; summary=[]
    for family,strategy,layout in cases:
        ratios=[]
        for pair in range(args.pairs):
            values={}
            for variant in (["baseline","candidate"] if pair%2==0 else ["candidate","baseline"]):
                start=time.perf_counter()
                row=json.loads(subprocess.check_output([str(Path(getattr(args,variant)).resolve()),family,strategy,layout,str(args.side),str(args.queries),str(args.seed)],text=True,timeout=90))
                row["process_seconds"]=time.perf_counter()-start; values[variant]=row
            a,b=values["baseline"],values["candidate"]
            for field in ("graph_hash","query_hash","cost_checksum","expanded","relaxed","pushes","heuristic_used","successful_routes"):
                if args.allow_bfs_work_change and strategy=="auto" and field in ("expanded","relaxed","pushes"): continue
                if a[field]!=b[field]: raise ValueError(f"behavior mismatch {field}: {family} {strategy}")
            if a["query_allocations"] or b["query_allocations"]: raise ValueError("query allocation")
            ratio=b["median_us"]/a["median_us"]; ratios.append(ratio)
            pairs.append({"family":family,"strategy":strategy,"layout":layout,"pair":pair,"median_ratio":ratio,**values})
        summary.append({"family":family,"strategy":strategy,"layout":layout,"median_pair_ratio":statistics.median(ratios),"min_ratio":min(ratios),"max_ratio":max(ratios),"all_ratios":ratios})
    output=Path(args.output); output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps({"metadata":metadata,"pairs":pairs,"summary":summary},indent=2)+"\n")
    human="Candidate/baseline latency ratios; shared-host descriptive paired evidence\n"+"\n".join(f"{s['family']:12s} {s['strategy']:8s} median_pair_ratio={s['median_pair_ratio']:.4f} range=[{s['min_ratio']:.4f},{s['max_ratio']:.4f}]" for s in summary)+"\n"
    output.with_suffix(".txt").write_text(human); print(human)
if __name__=="__main__": main()

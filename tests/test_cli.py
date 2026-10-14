#!/usr/bin/env python3
"""Strict parsing, smoke run, and exact deterministic replay."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path
binary = str(Path(sys.argv[1]).resolve())
valid = "rr1 4 4\n0 1 2\n1 3 2\n0 2 1\n2 3 8\n"
with tempfile.TemporaryDirectory() as directory:
    graph = Path(directory) / "graph.rr"
    def run(text, source="0", target="3", strategy="auto"):
        graph.write_bytes(text if isinstance(text, bytes) else text.encode())
        return subprocess.run([binary, str(graph), source, target, strategy, "--json"], capture_output=True, text=True)
    result = run(valid)
    assert result.returncode == 0, result.stderr
    data = json.loads(result.stdout)
    assert data["cost"] == 4 and data["path"] == [0, 1, 3]
    for _ in range(20):
        replay = json.loads(run(valid).stdout)
        assert {k:v for k,v in replay.items() if k != "cpu_seconds"} == {k:v for k,v in data.items() if k != "cpu_seconds"}
    for text in ["", "rr2 4 0", "rr1 0 0", "rr1 -4 0", "rr1 4294967295 0", "rr1 4 8000001", valid+"extra", valid.replace("0 1 2", "0 9 2"), valid.replace("0 1 2", "0 1 -2"), valid.replace("0 1 2", "0 1 4294967296"), valid[:-5], "rr1 "+"9"*140+" 0", "rr1 4 18446744073709551616"]:
        assert run(text).returncode == 2, repr(text)
    for bad in [b"rr1\x00garbage 4 0", b"rr1 4\x00garbage 0", b"rr1 4 1\n0 3 1\x00garbage\n"]:
        assert run(bad).returncode == 2
    assert run(valid, source="-1").returncode == 2
    assert run(valid, target="4").returncode == 2
    assert run(valid, strategy="bfs").returncode == 2
    assert run("rr1 4 0").returncode == 3
    assert json.loads(run(valid, strategy="astar").stdout)["heuristic_used"] == 0
print("CLI parser/replay checks passed")

#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

spec = importlib.util.spec_from_file_location('hex_run', Path(__file__).resolve().parents[1]/'benchmarks/hex_run.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
normal, frozen = map(lambda p: Path(p).resolve(), sys.argv[1:])
for binary, freeze in ((normal, False), (frozen, True)):
    results = module.run(binary, freeze)
    assert sum(r['verified_queries'] for r in results) == 576
for args in ([], ['4'], ['4', 'open', 'extra'], ['0', 'open'], ['4x', 'open'], ['999999999999999', 'open'], ['4', 'unknown']):
    assert subprocess.run([str(normal), *args], capture_output=True).returncode == 2
rows = [json.loads(s) for s in subprocess.check_output([str(normal), '4', 'open'], text=True).splitlines()]
for mutate in (lambda r: r.pop(), lambda r: r[0]['edges'].pop(),
               lambda r: r[1].update(cost=9), lambda r: r[1].update(query_allocations=1),
               lambda r: r[1].update(path=[99]), lambda r: r.append(r[1]),
               lambda r: r[1].update(heuristic_used=1-r[1]['heuristic_used']),
               lambda r: r[0].update(source_id='stale'), lambda r: r[0].update(clock='frozen'),
               lambda r: r[0].update(setup_ns=-1), lambda r: r[0].update(graph_bytes='invalid'),
               lambda r: [x.update(expanded=-1) for x in r[1:]]):
    bad = copy.deepcopy(rows)
    mutate(bad)
    try:
        module.validate(bad, 4, 'open')
    except (ValueError, KeyError):
        pass
    else:
        raise AssertionError('corrupt receipt accepted')
print('Hex fixtures: 1152 oracle-checked searches, frozen clocks, parser and corrupt-evidence checks passed')

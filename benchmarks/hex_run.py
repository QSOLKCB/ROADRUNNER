#!/usr/bin/env python3
"""Validate every hex route with an independent edge-relaxation oracle; retain receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import shlex
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ['benchmarks/hex_bench.c', 'benchmarks/hex_run.py', 'benchmarks/hex_source.py',
           'src/roadrunner.c', 'include/roadrunner.h', 'Makefile', 'CMakeLists.txt',
           'tests/test_hex_bench.py', '.github/workflows/native.yml']
INFINITY = 2**64 - 1


def sha(data):
    return hashlib.sha256(data).hexdigest()


def expected_edges(side, mode):
    # Coordinate-pair predicate, independent of the C direction-table loop.
    edges = []
    for u in range(side * side):
        q, r = u % side, u // side
        for vr in range(max(0, r-1), min(side, r+2)):
            for vq in range(max(0, q-1), min(side, q+2)):
                dq, dr = vq-q, vr-r
                if abs(dq)+abs(dr)+abs(dq+dr) != 2:
                    continue
                crosses = (q < side//2) != (vq < side//2)
                if crosses and (mode == 'cut' or (mode == 'wall' and (r != 0 or vr != 0))):
                    continue
                edges.append([u, vr*side+vq, 1])
    return sorted(edges)


def oracle(n, edges, source):
    # Bellman-Ford over original edges, neither runtime search nor a BFS clone.
    dist = [INFINITY] * n
    dist[source] = 0
    for _ in range(n-1):
        changed = False
        for u, v, cost in edges:
            if dist[u] != INFINITY and dist[u]+cost < dist[v]:
                dist[v] = dist[u]+cost
                changed = True
        if not changed:
            break
    return dist


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(rows, side, mode, frozen=False):
    require(len(rows) == 65, 'missing or extra records')
    graph, *queries = rows
    expected = expected_edges(side, mode)
    source_id = sha(''.join(sha((ROOT/p).read_bytes()) for p in ('benchmarks/hex_bench.c', 'src/roadrunner.c', 'include/roadrunner.h')).encode())
    require(graph['source_id'] == source_id, 'binary source identity mismatch')
    require(graph['clock'] == ('frozen' if frozen else 'CLOCK_MONOTONIC'), 'clock mode mismatch')
    require(type(graph['setup_ns']) is int and graph['setup_ns'] >= 0, 'setup timer')
    require(type(graph['graph_bytes']) is int and graph['graph_bytes'] > 0, 'graph memory')
    require(graph['kind'] == 'graph' and graph['side'] == side and
            graph['mode'] == mode and graph['nodes'] == side*side, 'graph identity')
    require(sorted(graph['edges']) == expected, 'graph topology/cost mismatch')
    arcs = {(u, v): c for u, v, c in expected}
    seen, distances, prior = set(), {}, {}
    for row in queries:
        repeat, query, algorithm = row['repeat'], row['query'], row['algorithm']
        require(repeat in (0, 1) and query in range(16) and algorithm in ('bfs', 'astar'), 'query identity')
        key = repeat, query, algorithm
        require(key not in seen, 'duplicate query')
        seen.add(key)
        s, t = (query*37+3) % (side*side), (query*53+side*side-1) % (side*side)
        if query == 0: s = t = 0
        if query == 1: s, t = 0, side*side-1
        if query == 2: s, t = side*side-1, 0
        require((row['source'], row['target']) == (s, t), 'query endpoints')
        require(row['order'] == ((algorithm == 'astar') - repeat - query) % 2, 'pair ordering')
        if s not in distances:
            distances[s] = oracle(side*side, expected, s)
        cost = distances[s][t]
        require(row['status'] == (4 if cost == INFINITY else 0) and row['cost'] == cost, 'oracle mismatch')
        path = row['path']
        if cost == INFINITY:
            require(path == [], 'unreachable route must be empty')
        else:
            require(path and path[0] == s and path[-1] == t and len(path) == len(set(path)), 'route endpoints/cycle')
            require(all((u, v) in arcs for u, v in zip(path, path[1:])), 'route edge')
            require(sum(arcs[u, v] for u, v in zip(path, path[1:])) == cost, 'route cost')
        require(row['query_allocations'] == 0 and row['heuristic_used'] == (algorithm == 'astar'), 'allocation/certificate contract')
        for field in ('expanded', 'relaxed', 'pushes', 'workspace_bytes'):
            require(type(row[field]) is int and row[field] >= 0, 'invalid counter: '+field)
        require(type(row['elapsed_ns']) is int and row['elapsed_ns'] >= 0, 'timer')
        if frozen:
            require(row['elapsed_ns'] == 0 and graph['setup_ns'] == 0, 'frozen clock')
        stable = {k: v for k, v in row.items() if k not in ('repeat', 'order', 'elapsed_ns')}
        if (query, algorithm) in prior:
            require(prior[query, algorithm] == stable, 'non-deterministic repeat')
        prior[query, algorithm] = stable
    require(len(seen) == 64, 'incomplete pair matrix')
    return {'graph': graph, 'graph_sha256': sha(json.dumps(expected, separators=(',', ':')).encode()),
            'queries': queries, 'verified_queries': len(queries)}


def run(binary, frozen=False):
    results = []
    for side in (4, 9, 24):
        for mode in ('open', 'wall', 'cut'):
            output = subprocess.check_output([str(binary), str(side), mode], text=True)
            results.append(validate([json.loads(line) for line in output.splitlines()], side, mode, frozen))
    return results


def command(args):
    p = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, check=False)
    return {'command': args, 'returncode': p.returncode, 'stdout': p.stdout, 'stderr': p.stderr}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compile-commands', required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    flags = args.compile_commands.resolve()
    # Actual build database bytes, not a caller's asserted compiler/flags label.
    database = json.loads(flags.read_text())
    require(any(Path(e['file']).name == 'hex_bench.c' for e in database), 'hex compile command missing')
    results = run(binary)
    report = {
        'schema': 'roadrunner.hex-fixture.v1',
        'scope': 'synthetic axial rhombi; shared-host characterization; no PCB or general speed claim',
        'timing': 'CLOCK_MONOTONIC nanoseconds; A* includes bounds generation and per-call certification; both include route reconstruction; graph/workspace construction is separate setup_ns; fixture generation, caller buffers, verification, JSON and teardown excluded',
        'source': {p: sha((ROOT/p).read_bytes()) for p in SOURCES},
        'git_head': command(['git', 'rev-parse', 'HEAD']),
        'git_status': command(['git', 'status', '--porcelain']),
        'binary_sha256': sha(binary.read_bytes()),
        'compiler_version': command([shlex.split(next(e['command'] for e in database if Path(e['file']).name == 'hex_bench.c'))[0], '--version']),
        'compile_commands': {'sha256': sha(flags.read_bytes()), 'entries': database},
        'host': {'uname': dict(zip(('system', 'node', 'release', 'version', 'machine', 'processor'), platform.uname())),
                 'python': sys.version, 'os_release': Path('/etc/os-release').read_text() if Path('/etc/os-release').exists() else None,
                 'kernel_package': None},
        'results': results,
        'verified_queries': sum(r['verified_queries'] for r in results),
        'interpretation': 'Raw zero and adverse timings retained. No ratios, percentile claims, speed threshold or automatic algorithm selection. Path equality is checked within each strategy, not across strategies.',
    }
    # Package identity is best effort: a container may use a host kernel absent
    # from its package database. Missing package evidence is explicit, not guessed.
    if platform.system() == 'Linux' and Path('/usr/bin/dpkg-query').exists():
        report['host']['kernel_package'] = command(['dpkg-query', '-W', '-f=${Package} ${Version} ${Architecture}\n', 'linux-image-'+platform.release()])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, separators=(',', ':'))+'\n')
    print(f"Verified {report['verified_queries']} hex queries; wrote {args.output}")


if __name__ == '__main__':
    main()

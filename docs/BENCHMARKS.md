# Reproducible benchmarks

The C harness uses CLOCK_MONOTONIC around search plus applicable heuristic generation/certification and path reconstruction. It generates fixed-seed unit/weighted grids, local grid queries, sparse directed graphs, a split disconnected grid and a corridor. Every output route is independently checked against input edges; unit grids/corridors also check analytical costs. The conformance oracle tests arbitrary small weighted graph optimality independently.

`benchmarks/run.py` runs each case in a fresh process, reverses case order on alternating repetitions, retains all microsecond samples, source commit/status/content digest, binary SHA-256, compiler/flags, platform/CPU and graph/query/cost checksums. Use the same parameters for comparisons. Quantiles are median sorted[n//2] and nearest-rank p95/p99; with 101/201 queries tails are descriptive, not reliable service-level estimates. Throughput counts successful routes only; disconnected tests report zero successful routes rather than treating failures as completed routes.

First-query timing means a fresh workspace/process, not flushed CPU caches or a cold OS page cache. Five queries warm the workspace; timed reported queries follow them. Warm query median/p95/p99, first query, generation, index+workspace construction, setup+first query, and whole subprocess wall time are distinct. Harness path verification is outside kernel timing but included in process wall time. No hidden offline heuristic certification is omitted.

Memory fields are exactly tracked requested bytes for graph/workspace/harness input, verification buffers and samples. Peak/steady refer to those live allocations, not process RSS, allocator metadata, stack or OS resident memory. Core graph/workspace bytes are separately reported. All query allocation counts must remain zero. No process-RSS claim is made.

## Matched geometry comparison

Row-major and Morton use identical logical power-of-two grids, edge costs and query coordinates. Logical graph/query and route-cost checksums must match. Morton changes physical ids, canonical neighbor order and heap ties, so this is explicitly a combined layout/traversal-order experiment, not a pure cache-locality measurement. Expansions/relaxations and setup costs are retained. Bit encoding supports side 4..512 here and requires no retained permutation tables; logical ids are decoded arithmetically for the route verifier. Conversion work is charged to generation/query setup.

## Run locally, including a future Ryzen 9 5950X

No GitHub service is required once the checkout is present. On the actual Linux workstation:

```sh
uname -a
lscpu
cc --version
make BUILD=build-ryzen CFLAGS='-O3 -DNDEBUG -std=c17 -Wall -Wextra -Wpedantic -Werror' test
python3 benchmarks/run.py --binary build-ryzen/rr_bench --output evidence/ryzen-5950x.json --side 64 --queries 201 --repeats 5 --seed 123456789 --compiler cc --flags='-O3 -DNDEBUG -std=c17 -Wall -Wextra -Wpedantic -Werror -Iinclude'
```

Confirm CPU identity from captured lscpu before naming results Ryzen. For a paired baseline/candidate comparison, build each exact source in a separate clean worktree with identical flags and run `benchmarks/compare.py` as documented in the experiment record. Never substitute `-march=native` in only one variant. Optional affinity/governor settings must be documented equally. Across different machines/compiler versions, compare workload validity and characterization, not attributable code speedups. No Ryzen measurements have been performed for this mission.

## CI

Official runner availability was checked 2026-10-14. Exact labels and references are in RESEARCH.md. Each job asserts `uname -m` and runner.arch, records actual compiler/OS/CPU/binary format and runs the binary natively. Linux x64 GCC/Clang, Linux ARM64 GCC, macOS ARM64/Intel AppleClang are targeted, with Linux Clang sanitizer coverage separate. The matrix has max-parallel 3 and ten-minute job limits; no paid runners or performance thresholds. PR checkout uses the exact head SHA rather than silently timing the synthetic merge commit. Actual coverage is established only by completed exact-head runs.

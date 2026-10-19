# ROADRUNNER

An experimental, portable C17 graph-routing library focused on **time to a valid
result**. Exact shortest paths, bounded reusable memory, and reproducible evidence
come before speed claims.

## What it does

- Immutable CSR graphs with directed edges, nonnegative 32-bit costs, and exact
  64-bit route totals. Parallel edges, self-loops, and zero-cost edges are supported.
- BFS, indexed-heap Dijkstra, and consistency-checked A*. AUTO uses BFS when every
  edge costs one; otherwise it uses Dijkstra.
- Reusable workspaces with no per-query allocation and deterministic paths for
  identical inputs and strategy. Each active query needs its own workspace.
- Optional copied, certified heuristic plans for repeated queries to one target.
  Ordinary A* still validates bounds on every call and safely falls back to zero
  bounds when they are absent or invalid.
- A strict command-line parser, independent shortest-path oracles, native CI,
  sanitizer tests, and source-bound benchmark receipts.

## Build and try it

You need a C17 compiler. Python 3 runs the tests and evidence tools; the library
and CLI have no external runtime dependency. The Make build also uses Python for
hex-benchmark source identification. Alternatively, use CMake 3.16+.

```sh
make test
printf 'rr1 3 3\n0 1 2\n1 2 3\n0 2 8\n' > graph.rr
build/rr graph.rr 0 2 auto --json
```

The route in this example has cost 5: `0 -> 1 -> 2`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The CLI accepts `rr FILE SOURCE TARGET {auto|bfs|dijkstra|astar} [--json]`.
Input starts with `rr1 node_count edge_count`, followed by exactly that many
`source destination cost` triples; node IDs are zero-based. CLI A* has no supplied
heuristic, so it explicitly uses zero fallback. Exit codes are 0 for a route,
3 for no path, and 2 for invalid input or an error.

## Library API

Include [`roadrunner.h`](include/roadrunner.h), create an immutable graph and a
workspace, call `rr_search`, then destroy both. Routes are written into
caller-owned buffers and include both endpoints. Insufficient capacity reports
the required length and valid cost without writing a partial path.

For repeated-target A*, `rr_heuristic_plan_create` copies and certifies the bounds
once; `rr_search_plan` reuses that certificate with the same exact search kernel.
The graph must outlive its plans, and each simultaneous query must use a separate
workspace. See [architecture and contracts](docs/ARCHITECTURE.md) and
[heuristic-plan ownership, costs, and measurements](docs/HEURISTIC_PLANS.md).

## Experiments and evidence

The [hex-grid fixture](docs/HEX_FIXTURE.md) compares exact A* with BFS on identical
unit-cost six-neighbor graphs. Open, wall, and disconnected cases are checked
against an independent Bellman–Ford oracle, with route validation, deterministic
replays, frozen-clock tests, and zero-allocation checks.

After the CMake build above, generate a verified receipt:

```sh
python3 benchmarks/hex_run.py --binary build/rr_hex_bench \
  --compile-commands build/compile_commands.json --output hex-fixture.json
```

Receipts retain raw query timings, graph/query data, compiled source identity,
verifier hashes, binary and build metadata, and OS/kernel identity. Missing kernel
package information is reported explicitly. The hex comparison includes A*
heuristic generation and certification in its query timer; it is not a complete
application-lifecycle benchmark or a PCB-routing implementation.

The existing [heuristic-plan lifecycle experiment](docs/HEURISTIC_PLANS.md) and
[recorded optimization experiments](docs/EXPERIMENTS.md) keep their separate
workloads, timing boundaries, failed hypotheses, and historical results. Shared
host timings characterize those workloads only. They do not establish a universal
speedup, an advantage from geometric cell area, or a result on your machine.

## Validation and limits

[Native CI](.github/workflows/native.yml) runs Linux x64 GCC/Clang, Linux ARM64 GCC,
macOS ARM64/Intel Clang, plus a leak-enabled ASan/UBSan lane. Check the
[actual runs](https://github.com/QSOLKCB/ROADRUNNER/actions/workflows/native.yml)
for the tested source and environment; a configured platform is not evidence of
a successful run. Tests supplement the documented contract, not a formal proof.

ROADRUNNER is a tested foundation, not a production security guarantee or a claim
to be the fastest routing engine. No dynamic graph service, GPU dependency,
bidirectional search, or concurrent bookkeeping engine is implemented. An
application graph corpus, local Ryzen characterization, and contention research
remain future work in the [bounded roadmap](docs/ROADMAP.md).

Further reading: [benchmark methodology and reproduction](docs/BENCHMARKS.md),
[research and geometry](docs/RESEARCH.md), [machine-oriented guide](README4AI.md),
and [contributor rules](AGENTS.md).

Licensed under [Apache-2.0](LICENSE). No third-party source is vendored.

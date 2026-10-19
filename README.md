# ROADRUNNER

An experimental, portable C17 graph-routing foundation focused on time to a valid result.

- Immutable CSR graphs with nonnegative 32-bit costs and 64-bit route totals
- BFS, indexed-heap Dijkstra, consistency-checked A*, and unit-cost automatic selection
- Reusable bounded workspace with zero query allocations and deterministic paths
- Strict CLI parsing, independent optimality tests, native CI and reproducible benchmark JSON

```sh
make test
printf 'rr1 3 3\n0 1 2\n1 2 3\n0 2 8\n' > graph.rr
build/rr graph.rr 0 2 auto --json
```

Or build with CMake 3.16+ and a C17 compiler. Python 3 runs integration tests and benchmark orchestration; the library/CLI need no external runtime. Apache-2.0 licensed; no third-party source is vendored.

Read [architecture](docs/ARCHITECTURE.md), [benchmarks and Ryzen commands](docs/BENCHMARKS.md), [research](docs/RESEARCH.md), and [bounded roadmap](docs/ROADMAP.md). This is a tested foundation, not a claim to be the fastest engine. Native coverage and measured results belong to exact CI runs and retained evidence, not runner-label assumptions.

Repeated-target workloads can opt into [reusable certified heuristic plans](docs/HEURISTIC_PLANS.md). Ordinary queries retain per-call validation; plan setup and memory costs are explicit.

The [synthetic hex-grid fixture](docs/HEX_FIXTURE.md) compares exact A* and BFS on
identical six-neighbor unit-cost graphs, with independent oracle checks and
source-bound receipts. It makes no PCB-routing or general speed claim.

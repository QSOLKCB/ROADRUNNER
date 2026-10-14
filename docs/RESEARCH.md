# ROADRUNNER independent research review

Research checked 2026-10-14. This is research/audit guidance, not implementation or performance evidence.

## Repository and OPT identity

ROADRUNNER was public and its main commit was `214ce725d4f98917dd6f88048c279efbda805e11`, containing only LICENSE and README.md when inspected. There was no existing AGENTS.md. OPT main was independently resolved to commit `ae58b8b52e1b50e5da754ecc37b62d430809b23a` (2026-10-08). Read AGENTS.md, README4AI.md, OPTIMIZATION-PROBLEM.md, CATALOG.md, EVIDENCE-SCOPE.md and the relevant records. Neither the catalog nor its historical formalization proves ROADRUNNER correct or fast.

Relevant transferable mechanisms:

- OPT-INV-001: name and test equivalence before reuse. Reusable scratch must reset all effective per-query state; epoch rollover must restore the invariant.
- OPT-PY-001: use bounded fixtures that preserve edge cases; do not reduce randomized trials if the count is part of a statistical claim. Retain independent reference semantics.
- OPT-PAR-001: bound CI/job concurrency; requested worker count is not evidence of actual concurrency. This does not justify runtime parallelism before measurement.
- OPT-BUDGET-001: proposed guidance, not a tested threshold. Shared runner timing is characterization, not a hard speed gate. Preserve raw samples and declare selection versus held-out confirmation if tuning candidates.
- OPT-PRUNE-001: heuristic bounds must be sound independently of the fast path. A* consistency is a sufficient, conservatively checkable gate.
- Keep OPT source timings out of ROADRUNNER speed claims. Copy mechanisms, not parameters or historical speedups.

Pinned source index: https://github.com/QSOLKCB/OPT/tree/ae58b8b52e1b50e5da754ecc37b62d430809b23a

## Native CI recommendation

Official GitHub runner documentation lists standard public-repository labels:

- ubuntu-24.04: x64, GCC and Clang jobs
- ubuntu-24.04-arm: arm64, GCC job (Clang optional bounded extra)
- macos-15: arm64, Apple Clang
- macos-15-intel: Intel x86_64, Apple Clang

ROADRUNNER is public, so the standard public runner eligibility applies. This is eligibility research, not proof a job ran. Avoid latest labels because they move. Use max-parallel 2 or 3, fail-fast false, fixed small datasets and bounded timeouts; no paid larger runners needed. An ordinary optimized build and actual tests/CLI execution are required on every target. Keep sanitizer and non-sanitizer benchmarks separate. A Linux x64 Clang ASan/UBSan job is a practical baseline sanitizer lane. Failures are failures; do not continue-on-error and call them passes.

Evidence commands: `uname -a`, `uname -m`, selected compiler `--version`, `cmake --version`, `python3 --version`, `git rev-parse HEAD`, `git status --porcelain`, Linux `lscpu` and `/etc/os-release`, macOS `sw_vers`, `sysctl -n machdep.cpu.brand_string`, `sysctl -n hw.physicalcpu`, `sysctl -n hw.logicalcpu`. Assert expected uname machine (`x86_64`, Linux `aarch64`, macOS `arm64`) and capture GitHub runner.arch. Capture `file` output for the executed binary, then execute it. Capture effective build flags/compile_commands.json, not just desired flags. Pin checkout to the actual PR head SHA for clean source attribution, or explicitly record merge SHA and both parents. PR head identity and default GITHUB_SHA can differ.

Official sources:
- https://docs.github.com/en/actions/reference/runners/github-hosted-runners
- https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md
- https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Arm64-Readme.md
- https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md
- https://github.com/actions/runner-images/blob/main/images/macos/macos-15-Readme.md

These image manifests are mutable. The job logs are authoritative for the image and compiler actually used.

## Geometry review

### Packed rectangular grid / CSR baseline

Workload: integer-cost routing over a fixed logical graph; use compact arrays and explicit CSR adjacency. A rectangular grid can also have implicit 4-neighbor adjacency, but comparing that with generic CSR changes both representation and neighbor-generation work. Row-major maps coordinates with arithmetic; no special spatial index is required. Store CSR offsets and destination/cost pairs plus query scratch. Four-neighbor Manhattan h = minimum_edge_cost * (abs(dx)+abs(dy)) is a lower bound when each edge changes one coordinate by one and costs at least that minimum. Zero-cost edges make the global-minimum bound zero. Teleports or cheaper diagonals invalidate that argument. Flat arrays support contiguous scans; heap traversal and irregular neighbor gathers need measurement rather than promised SIMD.

### Morton/Z-order

Workload: spatially local traversals of the SAME logical grid, with vertices permuted by interleaved coordinate bits. Store permutation and inverse permutation, or use documented bit encode/decode. Sorting/compacting non-power-of-two grid codes and rebuilding CSR are preprocessing, not free query work. Retain memory for mapping tables in the accounting. Encoding work may be batched, but irregular graph search is not automatically vectorizable. Bit encoding limits must be explicit; do not truncate large coordinates.

Critical confound: permuting physical IDs changes node-ID heap tie-breaking and possibly CSR neighbor order. Preserve canonical logical tie keys and logical neighbor order to isolate layout, or label the result combined layout/traversal-order and report node expansions/edges scanned. Keep query coordinates fixed, map route IDs back and verify route validity/cost independently. Compare both kernel timing and setup+query amortization. A cheap single-query task may never repay reindexing.

Primary implementation reference: https://github.com/Forceflow/libmorton (author-maintained, MIT). Encoding performance there is not ROADRUNNER route speed evidence. Original research: Bender et al., Optimal Cache-Oblivious Mesh Layouts, https://arxiv.org/abs/0705.1033v2, establishes model-specific memory-transfer results for mesh updates, not a general A* speedup. Original space-filling layout study: https://arxiv.org/abs/1612.06069, evaluates TU matrix decomposition, a different workload.

### Octahedral geometry / Manhattan distance

The 3D L1 unit ball is an octahedron. On six-axis-neighbor integer lattices, the sum of coordinate differences times a valid minimum step cost is a cheap admissible lower bound by the same per-edge argument. This is a metric property, not an octahedral runtime data structure. A finite octahedron surface graph is a different topology from a 3D lattice; a Manhattan bound must be verified against its actual edge weights. Coordinate storage and 6 neighbors per interior lattice vertex cost more than a 2D four-neighbor grid; changing topology does not demonstrate speed on the same workload. Arithmetic for bounds can be batched, while search still requires actual profiling.

A* original paper and correction are indexed by their author at https://ai.stanford.edu/~nilsson/publications.html. A machine-checkable sufficient condition is h(goal)=0 and h(u)<=w(u,v)+h(v) for every directed edge. Telescoping along any route proves h(u)<=route_cost, hence admissibility. Use checked arithmetic. Full certification is O(V+E) per goal unless a stronger graph-specific proof is carried by a trusted constructor; include it in end-to-end costs.

### Hexagonal axial grids

Workload: intrinsically six-neighbor planar cells, not arbitrary rectangular workloads. Store q,r; infer s=-q-r. Six fixed direction offsets give neighbors; h = min_cost * max(abs(dq), abs(dr), abs(dq+dr)) is the unobstructed hex step lower bound for that movement model. Include coordinate arithmetic/overflow checks and boundary lookup cost. Dense rhombus storage can be packed; irregular regions may need index maps. Independent vectorizable coordinate operations do not remove graph-search branches. Compare different layouts of the same hex graph, rather than claiming square-vs-hex timing proves a representation win.

Author-maintained implementation derivation: https://www.redblobgames.com/grids/hexagons/ (coordinate and distance code; no transplanted performance claim).

### Icosahedral spatial subdivision

Workload: spherical geospatial indexing, globe sampling and mesh adjacency. H3's maintained implementation documentation describes icosahedral face projection, hierarchical indexing and 12 unavoidable pentagons per resolution. Index/coordinate conversion and exceptional neighbors add preprocessing and control flow, with storage proportional to retained cells/adjacency. This is a credible domain-specific candidate; it is not a general routing accelerator. Compare identical spherical datasets/resolutions/error metrics and include conversion, index storage and neighborhood construction. Do not compare spherical versus planar graphs as if they were the same problem.

Primary implementation documentation: https://h3geo.org/docs/core-library/overview/ and https://h3geo.org/docs/core-library/coordsystems/ . No H3 dependency is needed for the first foundation.

### H4: 600-cell and 120-cell

Possible concrete experiment: route over explicitly generated polytope adjacency, store CSR, and compare alternative storage permutations of that same graph. The 600-cell has 120 vertices and 720 edges in Dechant's primary paper (2021); those small fixed graphs will fit caches and do not establish large-scale runtime benefits. The dual 120-cell must have independently validated adjacency if generated. Coordinates are four-dimensional and include algebraic structure; floating distance cutoffs need numerical-error checks or exact construction. Precompute adjacency at build time if experimenting, preserve source/license and graph checksum, and charge construction separately. Without a workload requiring that topology, reject mandatory integration. Dense coordinate math can be batched, but no route speedup follows from symmetry.

Primary research: Dechant, Clifford spinors and root system induction: H4 and the Grand Antiprism, https://arxiv.org/html/2103.07817v2 . This supplies geometry, not runtime benchmarks.

### E8 structures

Possible workload: explicitly declared nearest-neighbor/lattice/codebook search, with an exact mapping, distance model and recall/exactness contract. The E8 root system is finite and has 240 roots; an E8 lattice is an infinite discrete structure and must not be conflated with it. Store coordinates/adjacency for the concrete finite domain; 8D arithmetic and mapping/preprocessing/storage may dominate a small routing task. There is no evidenced general-purpose mapping from ROADRUNNER's arbitrary graph inputs to E8 that preserves shortest paths and improves time. Keep it research-only until such a mapping and same-workload measurement exist.

Primary paper: Dechant, The Birth of E8 out of the Spinors of the Icosahedron, https://arxiv.org/abs/1602.05985 . The user-supplied lead https://theoryofeverything.org/theToE/2026/06/26/e8-and-h4-hopf-fibrations-with-the-aizawa-chaotic-attractor/ was read: it presents visual integration of these geometries and contains no routing algorithm, benchmark conditions or measured runtime superiority.

### Aizawa synthetic trajectories

Optional future stress workload, not a correctness oracle. If used, retain differential-equation parameters, step size/integrator, initial conditions, sample count, graph-building rule and numeric precision. Floating trajectories can vary across toolchains; fixed generated input bytes/checksum are needed for truly same-workload performance comparisons. Do not introduce this complexity merely to satisfy a research theme.

## Audit gates

- Independent Bellman-Ford or Floyd-Warshall oracle over small directed graphs; not runtime Dijkstra reused as its own oracle.
- Test zero cost, parallel edges, self loops, disconnected graphs, source=goal, tie behavior, maximal uint32 cost, invalid vertex/range data, every allocation failure, and scratch reuse across varied goals and failure returns.
- Strict parsing must reject negative integers and overflow instead of accepting unsigned scanf/strtoull wraparound.
- Inconsistent/goal-nonzero heuristic must trigger a reported zero fallback. Consistent arrays with unreachable nodes near UINT64_MAX still need safe f addition.
- Do not rewrite predecessor on equal distance in zero-weight cycles unless acyclicity is proven.
- Median/p95/p99 depend on sample unit and sample count. A p99 from a tiny smoke sample is descriptive/order-statistic only; preserve raw samples, timer and quantile definition.
- A process RSS high-water value is not isolated per-variant memory. Report calculated owned bytes separately from measured process peak, with units and scope.
- Measurements bind source commit or source-content digest, exact compiler flags, binary identity, graph/query checksums, seed, setup boundaries, machine facts and sample count.
- Treat preallocated-query allocation counts separately from graph/scratch creation. Retain cold versus warm and preprocessing memory/time honestly.
- At most three substantial post-baseline experiments. Predeclare goal, risks, budget and accept/reject criterion; a failed or inconclusive hypothesis is a valid recorded outcome.

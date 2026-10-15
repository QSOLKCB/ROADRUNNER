# Architecture and contracts

## Core

C17 and compact arrays were selected because the empty starting repository imposed no compatibility constraints, the workload is scalar integer graph search, and dependencies/dispatch/allocation would otherwise dominate small queries. This is a falsifiable baseline, not proof C is universally fastest. CMake 3.16+ provides portable configuration; Make offers a direct compiler fallback.

The opaque immutable graph owns one allocation containing a header, `size_t offsets[n+1]`, and `(uint32 destination, uint32 cost)` arcs. The graph owns one allocation; libc qsort may use additional transient internal memory outside allocator callbacks. Input edge order is canonicalized by destination then cost within each CSR row. Parallel arcs, self loops and zero costs are valid. Nodes number 1 through UINT32_MAX-1 in count and use zero-based ids; UINT32_MAX is reserved. Every endpoint and allocation-size multiplication is checked before dereferencing/allocating. Graph input pointers must designate the stated number of elements; C cannot validate foreign buffer lengths.

The opaque workspace owns one reusable allocation, 32 bytes per node plus its header: distances/priority keys, parents, heap/FIFO, heap positions and generation marks. Lazy generation marks initialize only touched nodes; uint32 epoch rollover clears all marks, preserving the effective-reset invariant. Forced-rollover tests use epoch limit 3. The allocation is a bounded arena without growth on query. Memory allocation/release callbacks permit failure injection and accounting. The callbacks must be paired, return malloc-equivalent alignment and remain valid for the object's lifetime. There is no global mutable state. Graphs are shareable; each concurrent search needs a separate workspace. Output routes are caller-owned, include both endpoints, and never overlap input/scratch.

## Search and determinism

AUTO selects FIFO BFS only when every edge has cost one; otherwise indexed binary-heap Dijkstra. Explicit BFS rejects any other cost model. A* validates a provided nonnegative uint64 heuristic for the target on every call: h(target)=0 and h(u)<=cost(u,v)+h(v) for each arc. The check uses safe subtraction. Telescoping along a route proves admissibility; disconnected nodes may carry large heuristic values. Invalid or absent bounds are reported as zero fallback, preserving exact optimality. Heuristic generation and certification costs must be included in comparisons.

`rr_heuristic_plan_create` offers an opt-in repeated-target path. It copies all
bounds into one checked allocation, certifies that private copy and binds the
immutable graph and target. Invalid bounds are rejected rather than converted
to a zero-fallback plan. `rr_search_plan` invokes the same A* kernel and skips
only the already-proved certificate scan; it preserves tie rules, counters,
path/capacity semantics, epoch reset and zero query allocations. The ordinary
`rr_search` path still validates on every call. No public trust flag exists.

The graph is borrowed and must outlive its plans and all queries. Destroy plans
after active queries finish and before destroying the graph. Plan allocator
callbacks/context must remain alive until destruction. Plans are immutable and
shareable with separate exclusive workspaces. The caller may mutate or release
the input bounds after successful construction. Peak construction memory
includes both input and copied bounds; `rr_heuristic_plan_bytes` reports the
single requested allocation, not allocator overhead. A plan is not a cached
route and does not change the search ordering or heuristic quality.

Heap order is `(f, g, physical node id)`. Adjacency is sorted and parents change only on strict distance improvement; zero-weight cycles cannot rewrite equal-distance parents. BFS visits deterministic sorted neighbors and finishes on first goal discovery, whose hop cost is already optimal. Expanded counts popped nodes, so that fast path does not count the unpopped goal; pushes counts actual queue/heap insertions. Repeated identical input/strategy gives identical paths. Different strategies or physical permutations may return different equally optimal paths. Changing heap tie rules requires compatibility review.

A uint64 sentinel denotes infinity. With at most UINT32_MAX-1 nodes and uint32 nonnegative costs, a finite simple optimal path is strictly below UINT64_MAX. All relaxation additions are still checked. A* f addition saturates to UINT64_MAX; consistency ensures a node that can reach the goal has a bounded admissible priority. All authoritative g totals remain exact. There are no negative/float edge costs.

`RR_BUFFER_TOO_SMALL` returns the valid cost and required path length but writes no path. `RR_NO_PATH` has infinite cost/zero length. Invalid calls reset result evidence. Oversized/invalid graphs, allocation failures, unsupported BFS models and capacity failures are distinct status codes. Effective workspace state is reset on every valid search and is reusable after failures. API layouts are private except documented value types; version 0.1 is a foundation, not an irrevocable ABI stability promise.

## CLI and trust boundary

`rr FILE SOURCE TARGET {auto|bfs|dijkstra|astar} [--json]` accepts whitespace-separated `rr1 node_count edge_count`, then exactly edge_count source/destination/cost triples. It rejects negative/sign-prefixed/overflowed integers, overlong tokens, missing/trailing data, invalid ids and unsupported strategies. To bound untrusted text imports, the CLI limits nodes to 1,000,000 and edges to 8,000,000. The library does not impose those deployment-specific limits. CLI A* has no user heuristic and therefore explicitly uses zero fallback. The benchmark exercises certified Manhattan A*. Exit status: 0 route, 3 no path, 2 invalid/error. Timing is process CPU time for the CLI only; benchmark latency uses CLOCK_MONOTONIC.

## Validation and limits

The C conformance suite uses independent Floyd–Warshall reference distances, path-edge/cost validation, fixed seeds, adversarial cases, allocator injection and replay. Python exercises parser errors and stable JSON fields. ASan/UBSan supplement tests; none is a formal proof. No dynamic graphs, bidirectional routing, SIMD, multithreading, GPU, persistence format compatibility or production security guarantee is claimed.

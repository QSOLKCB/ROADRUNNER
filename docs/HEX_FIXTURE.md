# Synthetic hex-grid conformance fixture

This bounded follow-up implements the suggested equal-cost six-neighbor A* versus
BFS experiment. It does **not** implement PCB routing, compare square and hex cell
areas, or claim a routing-speed advantage from geometry. The core library, API,
heap tie rules and historical fixtures are unchanged.

## Workload and correctness

`rr_hex_bench` uses axial coordinates `(q,r)`, with both coordinates in `[0,side)`.
The six steps are `(1,0),(0,1),(-1,1),(-1,0),(0,-1),(1,-1)`; every directed edge
costs one. The sides are fixed at 4, 9 and 24. Each has three cases:

- `open`: all in-bounds six-neighbor arcs;
- `wall`: arcs crossing the middle q boundary removed except the bottom opening;
- `cut`: all middle-boundary crossing arcs removed, producing disconnected regions.

These are synthetic rhombi, not an application corpus. Sixteen deterministic
queries include source=target, both corner directions and varied endpoints.
Two repetitions alternate BFS/A* order, using separate exclusive reusable
workspaces. This is serial execution, not a scaling experiment.

A* generates `max(abs(dq),abs(dr),abs(dq+dr))` bounds. The bound changes by at most
one per retained edge, is zero at the target, and remains consistent when edges
are deleted. Ordinary `rr_search` still certifies it on every call. No plan-based
certificate reuse is mixed into this comparison. Both algorithms must return
optimal costs, but equally optimal paths can differ across algorithms.

Python reconstructs the expected topology via a separate coordinate-pair
predicate, checks every original edge, computes Bellman-Ford distances, and
checks every returned path, endpoint, cost, cycle absence, status and allocation
count. Repeats must preserve paths/counters within each algorithm. Tests also
exercise disconnected routes, the frozen timer, invalid fixture arguments,
and rejection of missing/corrupted evidence. Normal and frozen tests validate
1,152 searches in total. This does not replace the existing core conformance suite.

## Reproduce and interpret

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 benchmarks/hex_run.py --binary build/rr_hex_bench \
  --compile-commands build/compile_commands.json --output hex-fixture.json
```

Native CI includes this test on its existing matrix and retains a receipt per
native job. Sanitizer CTest runs the same fixture; no new platform is provisioned.

Raw `CLOCK_MONOTONIC` query timing includes route reconstruction. A* additionally
includes bounds generation and certification. Common graph/workspace setup is
reported separately. Fixture generation, caller-buffer allocation, verification,
serialization and teardown are excluded. This is a query-level characterization,
not an end-to-end lifecycle or throughput claim. Each observation is one query;
there are only two repetitions, no speed gate, and no claimed percentiles.
Zeros and adverse timings remain in the record. No algorithm is selected or
changed based on this fixture.

Receipts retain all original edges and raw query records, graph digests,
source-file hashes, local git head and dirty status, exact compilation database
contents/hash, compiler version, executable hash, OS release and kernel
release/version/machine. On Debian-style Linux, a best-effort exact running-kernel
package query retains its command, exit code and output. A container often has
no package matching the host kernel; a failed lookup is missing package evidence,
not proof of a package version. Other platforms have a null package record.
These are build receipts, not a full source/build/output SPDX inventory or kernel
attestation. Do not attribute differences to a kernel update without matched
build/workload/hardware evidence.

The separately suggested shared-versus-per-thread bookkeeping experiment is
deferred: ROADRUNNER shares immutable graphs but requires an exclusive workspace
per active query. It has no shared concurrent bookkeeping implementation to compare.
A contention study would need its own bounded design and evidence.

## Research provenance

- James Bowman's [hex-cell PCB routing article](https://excamera.substack.com/p/routing-pcbs-on-a-hex-cell-grid)
  was the original conversation lead. Its page could not be fetched during this
  implementation; no article results or code are reproduced here.
- [Red Blob Games, Hexagonal Grids](https://www.redblobgames.com/grids/hexagons/#distances)
  documents axial/cube coordinates and distance. It supplies the geometric
  derivation, not performance evidence for ROADRUNNER.

The executable also reports its compiled clock mode and a build-time SHA-256
identity over the benchmark, routing core and public-header bytes. The verifier
rejects a mismatching checkout or a frozen executable used as ordinary evidence.
The compilation database is retained as an observed build input; it is not a
cryptographic attestation that an arbitrary supplied database built that binary.
Use the fresh build commands above (as CI does). Python verifier/test hashes are
observed at verification time, separately from the compiled C source identity.

# Reusable certified heuristic plans

PR2 merged the core API, ownership contracts and conformance tests at
`322654a2e6a6d7a21e6e7c46bdc06980a52f6f6f` before this measurement slice
was published. This follow-up adds the benchmark, native test/evidence build
integration and measurements only; the routing core is unchanged from PR2.

A plan is useful when queries reuse one immutable graph and target. It copies
and certifies the caller's bounds once, then runs the existing exact A* kernel
without the O(V+E) per-call certificate scan. It does not improve the heuristic,
cache routes, change tie ordering, or make ordinary `rr_search` trust a caller.
See the public header for ownership, errors and concurrency requirements.

## Reproduce

```
make test
python3 benchmarks/plan_run.py --binary build/rr_plan_bench \
  --output plan-results.json --repeats 3 --compiler cc \
  --flags='-O3 -DNDEBUG -std=c17 -Wall -Wextra -Wpedantic -Werror -Iinclude'
```

The fixed protocol uses side 16 / seed 123456789 and held-out side 24 / seed
987654321, Q={1,10,100,1000}, with local-source weighted grids, dispersed-source
weighted grids, all-zero heuristic controls and unit grids. Manhattan bounds
are generated once for both legacy and planned A*. AUTO/BFS is a separate unit
reference, not an A* parity requirement. Process order alternates by repetition.
No seed/size tuning or warmups are performed. The two fixed grids are synthetic
controls, not a held-out real-application corpus or actual Ryzen execution.

A contiguous monotonic-clock lifecycle includes input generation, graph,
workspace and heuristic setup, plan copy/certification and input-h release,
queries and core/heuristic teardown. Common harness buffer allocation/free,
independent original-edge reverse-Dijkstra oracle, route verification and JSON
serialization are outside that interval and separately reported. All paths are
retained in preallocated harness buffers; query timing includes reconstruction.
Process wall time includes the complete process, including verification and
serialization. The separately measured component times are not substituted for
the contiguous lifecycle interval.

Requested memory includes harness input/oracle/path/sample buffers plus core
allocations. Constructor peak includes both caller bounds and plan copy. The
plan releases caller bounds after construction; ordinary A* retains them. This
is not RSS and excludes allocator metadata, stack and libc-internal allocations.
Hashes are replay diagnostics; every run separately checks shortest cost and
route edges. Plan/legacy statuses, costs, lengths and all search counters match.

## Local result, with limits

`evidence/heuristic-plans/local.json.gz` retains all 96 alternating pairs and 24
AUTO references. `summary.json` contains source digest, binary hash, compiler,
platform and summaries. The recorded source commit is the local PR1-equivalent
parent; its dirty status is explicit, and the source-content digest identifies
the candidate files actually compiled. Hosted receipts bind the final remote
commit directly. Earlier exploratory measurements are retained separately in
`exploratory.json.gz`; they predate small harness validation/CLI cleanups and
are not final-source validation.

On this shared Linux x86_64 GCC 14.2 host, held-out local-source plan/legacy
lifecycle ratios at Q=100 were 0.4130, 0.3641 and 0.4139 (median 0.4130), about
59% shorter measured lifecycle. At Q=1000 the median ratio was 0.2981. All three
held-out local Q=1 measurements were slower (median 1.0442). Q=10 had a favorable
median but did not improve in every pair; the first demonstrated all-pair
crossing for that workload was Q=100. This is workload-specific characterization,
not a one-shot, universal or production speed guarantee.

Weighted and all-zero controls showed weaker/noisier benefits; neither met a
repeatable 10% held-out improvement at any tested Q. All raw samples and adverse
ratios remain present. Zero-clock tests preserve zero samples and classify
unmeasurable comparisons as inconclusive; no epsilon or selected subset is used.
A discrete-Q crossing is not an analytical guarantee for intermediate/future Q.

For the 576-node held-out graph, the plan requested 4,656 bytes, adding 4,656
bytes at constructor peak and 48 bytes after releasing the 4,608-byte caller
array. No tracked query allocation occurred. The original baseline fixtures and
rejected four-ary heap evidence are unchanged.

The measurement design follows the setup-amortization and paired-confirmation
principles discussed for GLUBALL and GALAXY in `RESEARCH.md`; no donor code or
new geometry/random-search dependency is introduced. Further algorithm changes
require a separate evidence-led hypothesis and review. Real-application graphs
and actual Ryzen measurements remain open roadmap items.

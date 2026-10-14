# Initial post-baseline experiments

Baseline implementation local source commit: 43931e03f21c6de1bd40d38ea4d4aa9b4245b6ef; published as b9ecdef2991a803a3ce34230816f76f866d757b8 (same source bytes, baseline evidence added). Fixed seed 123456789, side 64, 201 timed queries, baseline 3 process repetitions. Shared Linux x86_64 host / GCC14.2 / -O3 -DNDEBUG C17; no exclusive-core guarantee. Full source digest and hardware/flags accompany evidence/baseline.json.

## Cycle 1: generation-marked workspace initialization

Research: local queries expand tens of nodes but reset all 4,096 node records; measured baseline local medians are roughly 4.9us (BFS) and 5.1us (Dijkstra). OPT-INV-001 requires reset equivalence, including rollover.

Hypothesis, declared before candidate measurement: lazy initialization with per-node uint32 generation marks removes O(V) per-query reset, improving local-query median by at least 20% over baseline in five alternating matched process pairs, while broad-case median regression stays within 10%. Cost: 4 extra bytes/node and a mark branch per touched neighbor. Rollover requires a full clear; tests force a small epoch limit. If broad workloads regress materially, reject or keep only as explicitly selected policy; do not silently change behavior contracts.

Validation: full oracle, parser/replay, allocator, ASan/UBSan; forced epoch rollover; all paired graph/query/route checksum and expansion counts must match exactly.

Measurement and decision: recorded below.

Cycle 1 selection evidence: candidate core at local 538b675, five AB/BA pairs in evidence/cycle1-generation.json.gz. Local BFS candidate/baseline median ratio 0.0444; local Dijkstra 0.0903. Largest broad median ratio 1.0252 (weighted A*). Workspace increases 4 bytes/node plus an 8-byte header pointer on this 64-bit ABI (16,392 bytes at 4096 nodes). All checksums/counters match. These large short-query differences support the declared mechanism on this host; tiny broad differences remain noise, not improvement claims. Held-out size128/seed987654321/101 queries/three pairs retained separately. Held-out confirmation and final decision are recorded below.

## Cycle 2: four-ary indexed heap

Research: weighted and sparse benchmarks are heap dominated; increasing heap arity trades more child comparisons for fewer levels, as conventional d-ary heap theory predicts. This is original implementation, no source copied. Hypothesis before measurement: four-ary heap improves weighted or sparse median at least 10% in five paired process repetitions versus cycle1 while no broad median regresses more than 10%. Identical priority comparator and decrease-key invariant must preserve paths/counters. Memory unchanged. If inconclusive or a regression, revert and retain the patch/results as research evidence.

Cycle 1 held-out result: local BFS ratio0.0156; local Dijkstra0.0306. Largest broad median ratio1.0097. Retain generation marks. Submicrosecond local timings are subject to timer granularity and shared-host noise; this is not a universal advertised speed factor.

Cycle 2 result at ac8e92f: weighted Dijkstra ratio1.0150, weighted A*1.0677, sparse Dijkstra1.0209, unit-grid Dijkstra1.0842. No target reached the ≥10% improvement criterion. Even unmodified BFS showed shifts, confirming compiler layout/shared-host confounders. Reject four-ary default; restore the generation-marked binary heap. Preserve evidence/cycle2-fourary.json.gz and .patch. No extra tuning of arity on this sample.

## Cycle 3: BFS finish on goal discovery

Research: with unit costs, FIFO search discovers nodes in nondecreasing hop count. Once the goal's first strict relaxation sets its parent, that cost is final and its deterministic path cannot change. Waiting to dequeue it does unnecessary work. Hypothesis before measurement: early completion improves local unit-grid median ≥20% across five alternating pairs versus retained cycle1, without >10% broad median regression. It may reduce expanded/relaxed/push counts; all route costs and paths must remain valid/deterministic. Weighted/A* logic remains untouched. No extra memory or preprocessing.

Cycle 3 result at 769d76c: local BFS median pair ratio0.5992, held-out size128/seed987654321 ratio0.6190. Largest broad selection median ratio1.0371; held-out1.0276. Independent conformance/benchmark oracles and route hashes pass; BFS work counts change intentionally. Retain. Improvements observed in untouched algorithms are confounded and are not attributed to this change.

All three allowed substantial cycles are closed. No further parameter search was done. Retained runtime is generation-marked binary-heap Dijkstra/A* plus BFS goal-discovery completion. Four-ary experiment is not compiled into the default. Reported ratios are descriptive local shared-host evidence, not general hardware guarantees.

## Reproduce a candidate without local-only commits

The immutable published baseline source is b9ecdef2991a803a3ce34230816f76f866d757b8. Apply evidence/cycle1-generation.patch to its core for cycle1; apply cycle2-fourary.patch on that state for the rejected cycle2, or cycle3-bfs-discovery.patch on cycle1 for the retained cycle3. Patches carry exact source content changes and receipts carry core SHA-256 plus binary SHA-256. Local experiment commit ids supplement this mapping; they are not presented as remote GitHub commits. Both binary paths must use the same unchanged C benchmark source and compiler options.

Example comparison once baseline/candidate binaries and core snapshots have been built:

```sh
python3 benchmarks/compare.py --baseline build-baseline/rr_bench --candidate build-candidate/rr_bench --baseline-commit BASELINE_COMMIT --candidate-commit CANDIDATE_COMMIT --baseline-core baseline/src/roadrunner.c --candidate-core candidate/src/roadrunner.c --output evidence/paired-local.json --flags='-O3 -DNDEBUG -std=c17 -Wall -Wextra -Wpedantic -Werror -Iinclude' --side 64 --queries 201 --seed 123456789 --pairs 5
```

Add --allow-bfs-work-change only when comparing the declared goal-discovery optimization; path-cost/checksum/reachability equivalence remains mandatory. Baseline/candidate receipt metadata records actual supplied source identities and flags; the caller must select the declared clean sources rather than mislabel a binary.

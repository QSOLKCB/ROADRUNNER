# ROADRUNNER independent audit

Audit date: 2026-10-14. Read-only reviewer; no repository changes, merges, releases, tags, permissions or workflow dispatches were performed by this reviewer.

## Result and identity

No blocking defect remains in the reviewed routing core. The initially discovered embedded-NUL parser acceptance and CLI timer-includes-formatting defects were corrected in the foundation. Benchmark interpretation was corrected to distinguish Morton layout plus traversal-order changes from an isolated locality experiment.

Published baseline: PR https://github.com/QSOLKCB/ROADRUNNER/pull/1 at `b9ecdef2991a803a3ce34230816f76f866d757b8`.
Local baseline `43931e03f21c6de1bd40d38ea4d4aa9b4245b6ef` was independently rebuilt; remote Git blob identities for core, public header, CLI, C tests, Python parser test and benchmark C source were checked equal. The published baseline source-content digest is `ae1a0f3299ca518b9ebc3eaf0b100719824945e0972180a38de507594370d56a`.

Reviewed optimized core: local commit `769d76cc8dcede7798fea125a811270a8dcb6d1e`, core SHA-256 `41e9f52a7f6a3113d61bf5da8d23bb44cabad924ae26fd9ea99711692534d2fa`. This includes generation marks and BFS goal-discovery completion, with the binary heap restored. Exact final published-head coverage must be checked separately from the baseline run below.

## Independent execution

On Linux x86_64 / GCC 14.2, immutable local archives of both baseline and optimized core passed the full repository suite: 262,228 queries and 6,224,084 checks, plus CLI parser/replay tests. Release builds retain checks under NDEBUG.

Additional independent tests, separately implemented for this audit:

- 240 fixed-seed directed multigraphs, 62,176 queries/capacity checks against Python arbitrary-precision Floyd-Warshall; source/goal variation, zero/max uint32 costs, duplicate edges, self loops, disconnected nodes, exact consistent heuristics, UINT64_MAX heuristics on unreachable regions, invalid-heuristic fallback, route edges/costs, acyclicity and buffer canaries.
- Optimized core repeated the same suite with epoch limits 1, 3 and UINT32_MAX while reusing one 16-node workspace across all varied-size graphs. Each run passed. Full route+cost digest `5ae468090dca5556096011a3cfaec8ecaaaecbdba371e9fba236f12514f85c93` matches baseline, supporting tested deterministic tie preservation.
- 108 native benchmark smoke cases: six families × three algorithms × two layouts × three seeds. Separate Python Bellman-Ford reconstructs original graph/query inputs and validates optimal-cost checksums, reachability, raw sample count, quantiles and zero query allocations. Baseline and optimized replay digest `16892c47d7ff3e8e355aee902b0832aa34adb2eb4719a961a5ad810a3c71f379` matches.

These finite tests and static checks are not formal verification or proof for all inputs.

## Native baseline CI: executed and verified

Run: https://github.com/QSOLKCB/ROADRUNNER/actions/runs/38536853070
Exact head: `b9ecdef2991a803a3ce34230816f76f866d757b8`.

- Linux x86_64: GCC 13.3.0, AMD EPYC 7763; native ELF binary, both CTest suites, benchmark generation passed.
- Linux x86_64: Clang 18.1.3, AMD EPYC 7763; native ELF binary, both CTest suites, benchmark generation passed.
- Linux aarch64: GCC 13.3.0, Neoverse-N2; native AArch64 ELF binary, both CTest suites, benchmark generation passed.
- macOS arm64: AppleClang 17.0.0.17000013, Apple M1 (Virtual); native arm64 Mach-O binary, both CTest suites, benchmark generation passed.
- macOS x86_64: AppleClang 17.0.0.17000013, Intel Core i7-8700B; native x86_64 Mach-O binary, both CTest suites, benchmark generation passed.
- Separate Linux Clang 18.1.3 ASan/UBSan lane: both CTest suites and a benchmark smoke execution passed; CTest enabled leak detection and halt-on-error.

The reviewer inspected actual logs: runner architecture assertions, compile flags, binary formats, tests and benchmark source/digest. This is native execution evidence, not cross-compilation or label-only inference.

All five artifact ZIPs were downloaded and their SHA-256 values matched GitHub metadata. ZIP CRCs, every extracted file hash, source SHA/digest, compiler flags and recomputed quantiles were checked. The five native outputs have identical logical graph/query/cost identities and deterministic work counters. Original artifacts expire around 2026-10-28; preserve the supplied extracted receipts and manifest in the repository for durability.

Artifact IDs: macOS arm64 `11881040465`; Linux GCC x64 `11880576429`; Linux GCC ARM64 `11880172608`; Linux Clang x64 `11880108036`; macOS Intel `11879824672`.

## Performance decisions audited

Raw paired receipts were inspected and their ratios recomputed; source/core/binary identities match independently built sources. Comparisons alternate AB/BA order, preserve exact workload hashes, use the same toolchain flags, include setup/memory fields, and keep every individual sample. Held-out size/seed runs are separate from the selection runs.

1. Generation marks (`538b675bff673fb0d7ff108957d767774c0e41d8`): retained. Five-pair local BFS/Dijkstra median ratios are 0.0444/0.0903 versus baseline; held-out ratios 0.0156/0.0306. Maximum broad median ratio is 1.0252 in selection and 1.0097 in held-out runs. Exact work/result counters match. Cost is an additional 4 bytes per node plus 8 header bytes on the measured 64-bit ABI. Rollover behavior was independently exercised.
2. Four-ary heap (`ac8e92f8c91760aa9cd6fc1c7b214ba3dd50d4c1`): rejected. Weighted Dijkstra 1.0150, weighted A* 1.0677 and sparse Dijkstra 1.0209 do not meet the predeclared 10% improvement threshold. Retain raw evidence and patch as a failed hypothesis, not an optimized default.
3. BFS goal discovery (`769d76cc8dcede7798fea125a811270a8dcb6d1e`): retained. Warm local BFS ratio 0.5992 in selection and 0.6190 in held-out runs; broad maximum medians 1.0371 and 1.0276. Unit-cost FIFO discovery proves the goal distance final at first discovery; strict parent updates preserve the prior path. Independent route digests support this property on tested inputs. Reduced expansions/relaxations/pushes are expected and correctly separated from route-equivalence checks.

These are descriptive shared-host warm-query results. Especially for submicrosecond queries, timer overhead, scheduling, CPU state and code-layout effects limit inference. They are not universal speed factors or confidence intervals. No isolated cache-locality benefit from Morton was established. Setup plus first-result and whole-process behavior must not be inferred from kernel medians; for example one cycle-3 pair improved warm median while first-result end-to-end time increased. Hardware-to-hardware timing is not an attributable code optimization comparison.

## Remaining limitations

- Final optimized published-head CI and final automated review need their own source-bound verification; baseline success cannot stand in for them.
- Requested-byte memory counters exclude process RSS, stack, allocator metadata and allocations not routed through the supplied tracker. They are not total process peak memory.
- Native coverage is the declared 64-bit Linux/macOS matrix. No Windows, 32-bit, other ISA, production workload, concurrency or formal-proof claim is made.
- H4/E8 remain research candidates without a justified general routing mapping or measured benefit. Only the matched rectangular/Morton experiment was implemented.
- Local host CPU identification initially failed through lscpu; later /proc/cpuinfo identifies a virtualized Intel Xeon Platinum 8573C. Preserve that as later supporting evidence rather than silently rewriting the original receipt.
- Automated reviewer summaries are not equivalent to formal verification. The initial mission prohibited merges; the user later explicitly authorized merging only after exact-head Codex reports No issues and applicable CI/independent gates pass. A stale baseline review does not meet that condition.
- No AMD Ryzen 9 5950X measurements have been executed. The documented commands prepare a future human-supervised run only.

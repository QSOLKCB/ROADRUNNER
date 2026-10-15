# ROADRUNNER final-head independent audit

Date: 2026-10-14. Read-only independent reviewer. This addendum supersedes the interim report's pending-final-CI and original no-merge-authorization statements; earlier technical observations remain historical evidence.

## Result

No unresolved actionable defect was found in the reviewed final candidate. This is an evidence-based finite audit, not formal verification or assurance for all inputs. Publication/merge approval is separate: the parent is resolving whether Codex's standard zero-finding wording satisfies the user's explicit review gate.

- PR: https://github.com/QSOLKCB/ROADRUNNER/pull/1
- Exact published head: `a9d780e4a4d314a62700a7a45aca87eb9f69276c`
- Exact tree: `7e43367add4cb7f8399cfaf3447b62a2b2e18367`
- Local immutable equivalent: `4d45ef20430e5583f3bb244aab07b973bcc67aae`
- All 54 published file blob identities and modes were compared with the local immutable tree and match.
- Source-content digest recorded by native CI: `be06a79552870eadcbe1d8aa516664b1e9748056f50fea93e84cfa6530459f95`
- Routing core SHA-256: `d04c584383d91b9c650e2dde6be203a831ea518d219b386b00f1cb8cdadb0bd3`

## Final correctness and timing regression

The retained core is generation-mark scratch reset plus binary-heap weighted search plus unit-cost BFS first-discovery completion. The final core differs from the previously audited optimized source only in compile-time epoch-limit bounds. Reused-workspace, forced-wrap, route/tie, allocation and independent oracle results are documented in the preceding audit.

Native macOS ARM CI exposed a real benchmark defect in the preceding head: a query batch can measure zero total elapsed time; dividing successful routes by zero emitted `inf` or `-nan`, which is not JSON. The reviewer independently reproduced this using a frozen clock. The fixed head preserves zero observations, emits JSON null for unmeasurable throughput, and never inserts a synthetic epsilon.

A related comparison defect was corrected: when either paired median is zero, its ratio is null and the complete case summary is `inconclusive_clock_resolution`. All raw observations and null ratios remain in the receipt; zero pairs are not silently discarded to create a favorable measured subset. Nonfinite/negative values are rejected.

The reviewer independently rebuilt the immutable final tree on Linux x86_64 / GCC 14.2 and passed:

- Normal and forced-epoch C suites, each 262,228 queries / 6,224,084 checks.
- 108 benchmark cases independently reconstructed and checked with Python Bellman-Ford.
- The same 108 cases with a frozen clock, validating optimal costs alongside zero times and null throughput.
- Comparison unit/mixed-zero cases plus a complete 30-pair frozen-clock integration across ten cases.
- CLI parser/replay checks, including embedded-NUL rejection.

The final independent local run is retained in `timing-fix-independent-make-test.log`. CMake was unavailable on the audit host; its complete path was actually exercised in the hosted native run below.

## Final native execution

Run: https://github.com/QSOLKCB/ROADRUNNER/actions/runs/38540648384
All six jobs succeeded at the exact published head above. The reviewer inspected actual job logs, native binary formats, architecture assertions, compiler identity/flags, executed tests and artifact contents.

| Platform | Compiler | Native binary | Job ID |
| --- | --- | --- | --- |
| Linux x86_64 | GCC 13.3.0 | x86-64 ELF | 115774252646 |
| Linux x86_64 | Clang 18.1.3 | x86-64 ELF | 115774252911 |
| Linux aarch64 | GCC 13.3.0 | AArch64 ELF | 115774252659 |
| macOS arm64 | AppleClang 17.0.0.17000013 | arm64 Mach-O | 115774252577 |
| macOS x86_64 | AppleClang 17.0.0.17000013 | x86_64 Mach-O | 115774252492 |
| Linux sanitizer | Clang 18.1.3 | x86-64 ELF; ASan/UBSan | 115774252223 |

Every job executed all six CTest cases: conformance, epoch_rollover, benchmark_oracle, benchmark_zero_clock, benchmark_comparison and cli_parser. The sanitizer lane enabled leak detection and halt-on-error. The five normal lanes additionally produced bounded native benchmark artifacts with release flags `-O3 -DNDEBUG -std=c17` verified in compilation receipts. These are genuine native executions, not cross-compiles or inferred runner-label coverage.

## Preserved final receipts

All five original ZIPs were downloaded. SHA-256 equals GitHub's artifact metadata; ZIP CRC integrity, every extracted member hash, source head/digest, compiler flags and recomputed raw timing quantiles were verified. Each artifact has twelve benchmark rows with 101 samples each; query allocations are zero. Across all five platforms the logical graph/query/cost hashes and deterministic work counters match. Performance values remain host-specific characterization, not cross-platform optimization comparisons.

Keep `native-final/manifest.json`, `native-final/job-evidence.json` and the original five ZIPs together. The manifest records every member hash, original artifact link and expiration. To avoid changing PR1's already reviewed head just to archive its own run, these source-pinned receipts may be committed in the next scoped PR; they remain evidence for PR1's exact head, not that later PR's changed code.

| Artifact ID | ZIP SHA-256 |
| --- | --- |
| 11882755629 | `a1d128a534ecccd204d9023a0287a55ef4d3b429a24c3274e1f4f514f3a45ef0` |
| 11882635854 | `ec331aeaabb10e0e21a01d0d3f6da507e77694707f47c8a6d308eb2171b97bbb` |
| 11881982470 | `af3b66e0467e888400a82e6c4b8474002be867492cb5efa2364d48de7790bba7` |
| 11881728317 | `c6604d7dbaf5f2f83ed3e912cb5d80b51c35ae2c93c8354ff88c04f02cfad023` |
| 11881494922 | `19d86e8dd61a1988cb88ddee872355263e732e04d059db4fc000d262f0542297` |

## Review gate and scope limits

At 09:18 UTC, Codex's exact-head replies identify `a9d780e4a4` and say “Didn't find any major issues”; latest: https://github.com/QSOLKCB/ROADRUNNER/pull/1#issuecomment-6165029668. No formal PR review records or inline review comments were returned. The code-review summary is source-bound to this final head; its security-review row still refers to the previous `be3e57c` head. The standard wording is not represented here as a literal “No issues” statement. The parent has asked the user whether it satisfies the requested merge gate; merge is held pending that answer. Sourcery's fresh review could not run because its existing review budget was exhausted; no paid upgrade was used.

Remaining evidence limits are explicit: shared virtual-host noise; warm-query versus setup/first-result distinction; requested-byte accounting excludes RSS/stack/allocator metadata/libc internals; Morton also changes physical tie/traversal ordering and therefore is not a pure locality isolation; A* consistency validation is charged per query; no measured H4/E8 benefit; no Windows/32-bit/other ISA/concurrent-use support claim; and no actual Ryzen 9 5950X execution. Exactly three post-baseline improvement cycles were measured: generation marks retained, four-ary heap rejected, BFS discovery retained. No further optimization cycle was smuggled into the timing-correctness repair.

The reviewer performed no external mutations, repository edits, comments, dispatches, merges, tags, releases, paid infrastructure changes or permission changes.

# Evidence index

- baseline.json / baseline.txt: original full three-repeat characterization; preserved unchanged. Original lscpu capture failed on a restricted sysfs mount; later paired receipts additionally capture readable /proc/cpuinfo (Intel Xeon Platinum 8573C) without rewriting that baseline.
- cycle1-generation.json.gz: selection pairs; cycle1-heldout.json.gz: independent seed/size confirmation. Retained generation marks.
- cycle2-fourary.json.gz: rejected heap experiment; source patch retained.
- cycle3-bfs-discovery.json.gz and cycle3-heldout.json.gz: retained unit-cost early completion, with intentional work-count differences.
- .txt files: compact human summaries. .patch files reproduce experimental cores from the published baseline and cycle1 state.
- manifest.json: compressed/uncompressed SHA-256 identities for every compressed raw receipt. No samples were dropped. `gzip -dc FILE.json.gz` emits JSON.

Benchmark-only local commits are recorded as local identities and paired with exact core/binary SHA-256. Published baseline is b9ecdef2991a803a3ce34230816f76f866d757b8. Patch replay and source hashes are the portable reproduction route. Measurements are shared-host characterizations, not hardware speed guarantees. They are not Ryzen results.

Native baseline ZIPs are durably retained under native-baseline/ with independently verified GitHub artifact and member hashes. final-local.json/txt characterizes the retained final core; source digest binds actual measured implementation bytes. Final hosted improvements require their own exact-head CI run and are not inferred from baseline receipts.

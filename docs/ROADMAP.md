# Bounded roadmap

1. Foundation: C17 CSR, BFS/Dijkstra/A*, safe parsing, independent oracle, bounded reusable workspace, native CI and measured evidence.
2. Evidence-led improvements: at most three initial hypotheses; preserve baseline and failed/inconclusive outcomes. No universal speed claims from shared hosts.
3. Next only after review: held-out application graph corpus and local Ryzen characterization; then investigate reusable certified heuristics, bounded-weight queues or bidirectional search where measurements justify them.

Not in this mission: mandatory H4/E8, GPU/runtime frameworks, dynamic graph service, TRIALITY/TRINITY integration or release engineering. The initial mission prohibited merges; later explicit user authorization permits gated merges after exact-head Codex No issues and passing applicable validation. Continue only with scoped, evidence-supported PRs.

Current scoped implementation: reusable copied heuristic plans for repeated targets, sharing the existing A* kernel. See HEURISTIC_PLANS.md for lifecycle measurements and limits. Synthetic held-out grids do not complete the application-corpus/Ryzen work above. No additional search-order hypothesis is included.

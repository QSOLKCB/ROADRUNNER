# ROADRUNNER contributor rules

- Objective: shortest time to a valid result. Correctness is a contract, not a benchmark option.
- C17 core, no mandatory external runtime/network/GPU dependency. Keep the public API documented.
- Read README4AI.md and docs/ARCHITECTURE.md before changing routing behavior.
- Run `make test` (or CMake + CTest), then sanitizers on supported hosts. Tests must remain enabled with NDEBUG.
- Compare shortest paths against an independent oracle; preserve deterministic tie rules, overflow and allocation handling.
- No per-query allocation. Reusable workspaces are exclusive to one active query.
- Benchmark improvements against matched inputs, compiler flags and hardware. Preserve raw samples, failed hypotheses and source identity. Shared-host timing is characterization, not a performance guarantee.
- Geometry is optional research. Do not infer runtime superiority from mathematical elegance.
- Keep PRs reviewable. Do not merge, release, tag, change permissions or provision paid infrastructure without explicit user authorization.

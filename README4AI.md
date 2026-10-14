# Machine-oriented guide

ROADRUNNER is an experimental C17 directed graph-routing library with a strict integer-cost model. Read include/roadrunner.h for the API and docs/ARCHITECTURE.md for invariants. The original upstream tree was 214ce725d4f98917dd6f88048c279efbda805e11 (LICENSE and README only).

Build: `make test`, or `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure`.

Local sanitizer (set ASAN_OPTIONS=detect_leaks=0 only where ptrace blocks LeakSanitizer; hosted CI enables leak detection): `make BUILD=build-san CFLAGS='-O1 -g -std=c17 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test`.

Python 3 is only needed for CLI integration tests/evidence orchestration. No pip packages are required. Benchmark latency includes path reconstruction; A* includes heuristic generation/validation. Constructor memory and preprocessing are separate fields. Read docs/BENCHMARKS.md before drawing conclusions. No performance threshold in noisy shared CI.

The initial foundation mission did not authorize merges. Later explicit user authorization permits merging only after exact-final-head Codex reports No issues, applicable CI passes and independent review is clear; a stale review or CI alone does not qualify. No TRINITY/TRIALITY integration is implemented.

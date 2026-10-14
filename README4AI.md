# Machine-oriented guide

ROADRUNNER is an experimental C17 directed graph-routing library with a strict integer-cost model. Read include/roadrunner.h for the API and docs/ARCHITECTURE.md for invariants. The original upstream tree was 214ce725d4f98917dd6f88048c279efbda805e11 (LICENSE and README only).

Build: `make test`, or `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure`.

Local sanitizer: `make BUILD=build-san CFLAGS='-O1 -g -std=c17 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test`.

Python 3 is only needed for CLI integration tests/evidence orchestration. No pip packages are required. Benchmark latency includes path reconstruction; A* includes heuristic generation/validation. Constructor memory and preprocessing are separate fields. Read docs/BENCHMARKS.md before drawing conclusions. No performance threshold in noisy shared CI.

Changes are not authorized to merge by the autonomous foundation mission. Do not infer a merge grant from passing CI or review. No TRINITY/TRIALITY integration is implemented.

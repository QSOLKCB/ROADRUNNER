CC = cc
CFLAGS = -O3 -DNDEBUG -std=c17 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS = -Iinclude
BUILD ?= build
.PHONY: all test clean
all: $(BUILD)/rr $(BUILD)/rr_tests $(BUILD)/rr_tests_wrap $(BUILD)/rr_bench $(BUILD)/rr_bench_zero_clock
$(BUILD):
	mkdir -p $@
$(BUILD)/rr: src/cli.c src/roadrunner.c include/roadrunner.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/cli.c src/roadrunner.c $(LDFLAGS) -o $@
$(BUILD)/rr_tests: tests/test_roadrunner.c src/roadrunner.c include/roadrunner.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_roadrunner.c src/roadrunner.c $(LDFLAGS) -o $@
$(BUILD)/rr_bench: benchmarks/bench.c src/roadrunner.c include/roadrunner.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) benchmarks/bench.c src/roadrunner.c $(LDFLAGS) -o $@
$(BUILD)/rr_tests_wrap: tests/test_roadrunner.c src/roadrunner.c include/roadrunner.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRR_EPOCH_LIMIT=3 tests/test_roadrunner.c src/roadrunner.c $(LDFLAGS) -o $@
$(BUILD)/rr_bench_zero_clock: tests/bench_zero_clock.c benchmarks/bench.c src/roadrunner.c include/roadrunner.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/bench_zero_clock.c src/roadrunner.c $(LDFLAGS) -o $@
test: all
	$(BUILD)/rr_tests
	$(BUILD)/rr_tests_wrap
	python3 tests/test_benchmark_oracle.py $(BUILD)/rr_bench
	python3 tests/test_benchmark_oracle.py $(BUILD)/rr_bench_zero_clock --zero-clock
	python3 tests/test_benchmark_comparison.py $(BUILD)/rr_bench_zero_clock
	python3 tests/test_cli.py $(BUILD)/rr
clean:
	rm -rf build

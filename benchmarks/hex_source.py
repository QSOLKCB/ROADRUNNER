#!/usr/bin/env python3
"""Build-time identity of the exact C/header inputs; matches CMake's recipe."""
import hashlib
from pathlib import Path
root = Path(__file__).resolve().parents[1]
parts = ''.join(hashlib.sha256((root/p).read_bytes()).hexdigest() for p in
                ('benchmarks/hex_bench.c', 'src/roadrunner.c', 'include/roadrunner.h'))
print(hashlib.sha256(parts.encode()).hexdigest())

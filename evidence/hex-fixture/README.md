# Hex fixture local receipt

`local-2026-10-19.json` is a local shared-container characterization of 576
oracle-validated searches across 9 synthetic graph cases. The build was CMake
Release; exact compile database, compiler version, binary and input-source hashes
are retained in the JSON. Its base git head and dirty status describe the
pre-publication checkout; the embedded compiled source ID and per-file hashes
identify the tested inputs. Publication has a separate commit identity.

Receipt SHA-256: `f2216a783fa91bfd7e56257369140a06ecd72e102843ea891860405bd80d7044`.

Validation performed before publication:

- Full Make release suite passed, including 1,152 normal/frozen hex queries.
- CMake Release CTest: 8/8 tests passed.
- Full Make ASan/UBSan suite passed with leak detection disabled because ptrace
  blocks LeakSanitizer locally; the hosted sanitizer lane keeps leak detection on.
- Independent review found two evidence-validation gaps (metric types/ranges and
  stale/frozen binary attribution); both were corrected and reproduced as rejected.

This receipt is not native-matrix evidence, a speed guarantee, a kernel-package
attestation or a PCB workload. The kernel-package command failed because the
container package database has no matching running-kernel package; this failure
is preserved. Read docs/HEX_FIXTURE.md for exact timing boundaries and limits.
All previous repository evidence remains unchanged.

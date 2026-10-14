# Native validation receipts

The baseline run [38536853070](https://github.com/QSOLKCB/ROADRUNNER/actions/runs/38536853070) executed source b9ecdef2991a803a3ce34230816f76f866d757b8. All six jobs succeeded:

- ubuntu-24.04 / GCC: native x86_64, GNU13.3
- ubuntu-24.04 / Clang: native x86_64, Clang18.1.3
- ubuntu-24.04-arm / GCC: native aarch64, GNU13.3
- macos-15 / Clang: native arm64, AppleClang17.0.0.17000013
- macos-15-intel / Clang: native x86_64, AppleClang17.0.0.17000013
- ubuntu-24.04 sanitizer: Clang ASan/UBSan with leak detection

Architecture assertions, executable formats, actual native execution, tests and benchmark generation were independently checked from job logs. The five original GitHub artifact ZIPs are preserved under evidence/native-baseline/ beyond the hosted artifact retention window. manifest.json records artifact IDs, source/ZIP/member hashes, CPU and compiler identities. Each ZIP hash was verified against GitHub's artifact digest and archive integrity was tested. job-evidence.json retains relevant job log lines. Cross-architecture performance differences are not code speedup evidence.

Final improvements add forced-epoch-rollover and independent generated-workload Bellman-Ford tests to every native lane. Their exact-final-head run must be checked separately; the baseline success above does not establish later source coverage. The final PR review/closeout supplies that exact run identity. Local GCC14.2 validation of the retained final core passed both 262,228-query suites, 108 generated benchmark oracle cases, CLI tests and ASan/UBSan; local leak detection is unavailable under ptrace, so hosted leak coverage matters.

# p101-inspect

`p101-inspect` is the native capture, analysis, model, presentation, and bounded
interleaving tool. `inspect-capture` remains the small policy-free subprocess
boundary; shared event parsing, lifecycle mechanics, analysis, receipts, and
lesson routing live in `lib_tool_event`.

Exit status is zero for a clean command, one when the command or captured
policy reports findings, and two when capture cannot produce trustworthy
evidence.

```sh
./build-clang/p101-inspect run -o /tmp/run -- ./program argument
./build-clang/p101-inspect model verify /tmp/run/analysis
./build-clang/p101-inspect view report /tmp/run/analysis
```

Admitted inputs are versioned `lib_tool_event` call/resource streams, capture
receipts, sanitizer text, expectation files, and named built-in rule packs.
Outputs are text and JSON policy reports, the causal model, a finding index,
fingerprinted receipts, and conventional exit statuses. The tool cannot recover
direct libc calls, third-party internals, missing events, unexecuted paths, or
unmodeled scheduler behavior. Interleaving exploration is bounded and reports
counterexamples; it is not an exhaustive concurrency proof.

## Evidence

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DP101_BUILD_LEVEL=1
cmake --build build
cmake -S . -B build -DP101_BUILD_LEVEL=2 && cmake --build build
```

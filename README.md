# p101-inspect

`p101-inspect` owns policy-free capture. `inspect-capture` runs one command and
writes immutable event streams, command output, a manifest, and a fingerprinted
receipt. Replay and presentation policy lives in `scripts/runtime/`; event
parsing and lifecycle mechanics live in `lib_tool_event`.

Exit status is zero for a clean command, one when the command or captured
policy reports findings, and two when capture cannot produce trustworthy
evidence.

```sh
./build-clang/inspect-capture -o /tmp/run -- ./program argument
../../scripts/runtime/p101-analyze.py /tmp/run
```

The capture can only contain events emitted by instrumented wrappers. It cannot
recover direct libc calls, third-party internals, missing events, or paths the
command did not execute.

## Evidence

```sh
./change-compiler.sh -c clang
./build.sh
./test.sh
```

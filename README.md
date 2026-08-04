# p101-observe

`p101-observe` is the evidence-capture component for the Programming 101
runtime tools. It runs one command with p101 resource and call logging enabled
and captures the command output and immutable event streams.

It deliberately does not analyze. It records the command, stdout, stderr,
resource and call streams, manifest, and receipt.
The resulting immutable directory can be analyzed later with `p101 analyze`:

    p101 observe -o run-capture -- ./my-program config.txt
    p101 analyze -o run-analysis run-capture

The replay command verifies the capture receipt and artifact fingerprints
before and after analysis, builds one shared model, runs the policy modules,
and writes a separate analysis receipt. It refuses incomplete or modified captures unless
the caller explicitly uses `--force`, which is permanently recorded in the
analysis receipt. `p101 run` composes capture and analysis for convenience.

## Usage

    p101-observe [-h] [-v] [-A] [-R] [-o <capture-dir>] -- <command> [args...]

Examples:

    p101-observe -- ./my-program config.txt
    p101-observe -o run-capture -- ./my-program

With no `-o`, the report directory is `p101-observe-<pid>` in the current
directory. The directory must not already exist.

## Capture contents

`p101-observe` writes:

    command.txt
    stdout.txt
    stderr.txt
    resources.log
    calls.log
    manifest.txt
    receipt.txt
    tool-receipt.json
    summary.txt

The observed command receives these environment settings:

    P101_RESOURCE_LOG=<report-dir>/resources.log
    P101_CALL_LOG=<report-dir>/calls.log
Call arguments and results are redacted by default. `-A` opts in to argument
values and `-R` opts in to return values; either may contain credentials,
paths, personal data, or other secrets. The manifest records which value
classes were enabled. Programs needing field-specific redaction can install a
custom `p101_env_call_observer`; the built-in text logger intentionally does
not guess which bytes are sensitive.

Those are set in the child process immediately before `exec`, after
`p101-observe` has redirected stdout and stderr, so the report is about the
target program rather than the launcher setup.

`manifest.txt` records the command and primary evidence paths for
reproducibility, including the p101 event schema, generation time, and host
platform. The log contract is owned by
[`lib_tool_event`](https://github.com/programming101dev/lib_tool_event/blob/main/docs/event-format.md).
`receipt.txt` binds a run id to command status and bounded fingerprints of the
admitted logs. It uses FNV-1a 64 for inexpensive change
detection, not cryptographic authenticity. Raw TSV events are not hash-chained
or synchronously flushed per event.
`tool-receipt.json` uses the shared `lib_tool_event` run-receipt contract. It
binds the tool, capture policy, run identity, outcome, and the fingerprint of
`receipt.txt` in a machine-readable record. Because `receipt.txt` fingerprints
every admitted capture artifact, that binding covers the complete admitted
evidence set. The v4 semantic digest makes tampering or accidental edits
detectable with `p101-tool-receipt verify tool-receipt.json`. The receipt states
its non-proofs; its FNV digests are not authenticity signatures.
The event log also has a
teaching exercise in [`docs/event-log-assignment.md`](docs/event-log-assignment.md).

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | The command exited cleanly and the capture receipt was written |
| `1` | The observed command failed |
| `2` | `p101-observe` could not complete the capture workflow |

## Boundaries

`p101-observe` sees the p101 event streams emitted by the observed process. It
does not magically observe direct libc calls, allocations or descriptors created
inside unwrapped third-party code, or resources whose wrappers did not emit
events. Its report directory is a deterministic receipt for one command run, not
a proof that all executions of the program are clean.

## The workflow

1. **Configure** — `./change-compiler.sh -c clang` picks the compiler and
   configures the build.
2. **Build** — `./build.sh` runs the strict analysis build.
3. **Test** — `./test.sh` runs the Unity tests.
4. **Gate** — `./check.sh` runs format, build, tests, and a fuzz smoke.

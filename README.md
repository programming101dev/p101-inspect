# p101-observe

`p101-observe` is the front door for the Programming 101 runtime tools. It runs
one command with p101 resource and call logging enabled, captures the command's
output, runs `p101-resource-tracker`, `p101-trace`, and `p101-report`, and
leaves a small report directory behind.

It does not replace the lower-level tools. It bundles their ordinary workflow so
students do not have to remember the environment variables and follow-up
commands.

## Usage

    p101-observe [-h] [-v] [-o <report-dir>] [-r <p101-resource-tracker>] [-t <p101-trace>] [-p <p101-report>] -- <command> [args...]

Examples:

    p101-observe -- ./my-program config.txt
    p101-observe -o run-report -r ../p101-resource-tracker/build-clang/p101-resource-tracker -t ../p101-trace/build-clang/p101-trace -p ../p101-report/build-clang/p101-report -- ./my-program

With no `-o`, the report directory is `p101-observe-<pid>` in the current
directory. The directory must not already exist.

## Report contents

`p101-observe` writes:

    command.txt
    stdout.txt
    stderr.txt
    resources.log
    calls.log
    resource-report.txt
    resource-report.json
    resource-tools.stderr.txt
    trace-tree.txt
    trace-summary.txt
    trace-tools.stderr.txt
    correlated-report.txt
    report-tools.stderr.txt
    summary.txt

The observed command receives these environment settings:

    P101_RESOURCE_LOG=<report-dir>/resources.log
    P101_CALL_LOG=<report-dir>/calls.log
    P101_CALL_LOG_ARGS=1
    P101_CALL_LOG_RESULT=1

Those are set in the child process immediately before `exec`, after
`p101-observe` has redirected stdout and stderr, so the report is about the
target program rather than the launcher setup.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | The command exited `0`, the reports were produced, and no resource findings were parsed |
| `1` | The command failed, `p101-resource-tracker` reported resource findings, or `p101-report` reported correlated findings |
| `2` | `p101-observe` could not create/run the report workflow |

## The workflow

1. **Configure** — `./change-compiler.sh -c clang` picks the compiler and
   configures the build.
2. **Build** — `./build.sh` runs the strict analysis build.
3. **Test** — `./test.sh` runs the Unity tests.
4. **Gate** — `./check.sh` runs format, build, tests, and a fuzz smoke.

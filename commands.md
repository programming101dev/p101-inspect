# Commands

Quick reference for `inspect-capture`. Every script also supports `--help`.
Run `cmake -S . -B build -DCMAKE_C_COMPILER=<compiler> -DP101_BUILD_LEVEL=1` once before building.

## Running the tool

| Command | What it does |
| --- | --- |
| `inspect-capture -- ./prog` | Run `./prog` and capture p101 resource/call logs |
| `inspect-capture -o capture -- ./prog arg` | Write immutable evidence to `capture/` |
| `p101-inspect run -o analysis -- ./prog` | Capture and analyze through the canonical native composition |
| `inspect-capture -v -- ./prog` | Verbose: trace `inspect-capture` itself |

Each capture directory includes `manifest.txt` for reproducibility,
`receipt.txt` for bounded run identity, status, and artifact fingerprints, and
the two plain-text event streams.

Exit status: `0` clean command, `1` command failure, `2` capture trouble.

## Building and checking

| Command | What it does |
| --- | --- |
| `cmake -S . -B build -DCMAKE_C_COMPILER=<cc> -DP101_BUILD_LEVEL=1` | Configure the build with a compiler (also `set CMAKE_C_COMPILER=<cc>`). `--help` lists detected compilers. |
| `cmake -S . -B build -DCMAKE_C_COMPILER=<cc> -DP101_BUILD_LEVEL=1 -s address,undefined` | Configure with specific sanitizers |
| `cmake -S . -B build -DCMAKE_C_COMPILER=<cc> -DP101_BUILD_LEVEL=1 --coverage` | Configure an instrumented build for coverage (gcov) |
| `cmake --build build` | Strict analysis build: format-check, clang-tidy, cppcheck, static analyzer, `-Werror`, sanitizers. `-q` = quiet |
| `cmake --build build --target format` | Auto-fix in place: clang-tidy `--fix` + clang-format |
| `clang-format --dry-run --Werror -style=file <sources>` | Format check only, no build (hook-friendly); non-zero if unclean |
| `cmake -S . -B build -DP101_BUILD_LEVEL=3 && cmake --build build` | Format + strict build + tests + fuzz smoke -> one PASS/FAIL. `--cov <pct>` adds a coverage gate |
| `cmake -S . -B build -DP101_BUILD_LEVEL=2 && cmake --build build` | Build and run the Unity test suite |
| `../../scripts/update-all.sh --level 2` | Run the tests across every supported compiler |
| `configure and run the fuzz/ CMake project` | Fuzz the argument parser (coverage-guided + sanitizers); PASS/FAIL. `-t <secs>` sets the time budget |
| `configure with -DP101_COVERAGE_MODE=ON and run gcovr` | HTML coverage report. `--report-only` skips the run; `--min <pct>` fails under a threshold |
| `configure with -DP101_COVERAGE_MODE=ON and run gcovr` \| `profile` | One entry point for the coverage / profiling reports |
| `cmake -S . -B build` | Report what actually works on this machine for this project |
| `cmake --build build --target clean` | Remove `build-` / `coverage-` / `profile-` output (`-n` previews) |

Less common: `../../scripts/update-all.sh --level 1` (build with every compiler), `cmake -S . -B build`
(detect installed compilers), `cmake -S . -B build` (verify required tools).

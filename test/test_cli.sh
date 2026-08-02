#!/usr/bin/env bash
set -euo pipefail

tool=$1
true_path=$(command -v true)
false_path=$(command -v false)
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-observe-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

expect() {
  wanted=$1
  shift
  set +e
  "$tool" "$@" >"$work/stdout" 2>"$work/stderr"
  got=$?
  set -e
  [ "$got" -eq "$wanted" ] || {
    printf 'expected %s, got %s: %s\n' "$wanted" "$got" "$*" >&2
    cat "$work/stderr" >&2
    return 1
  }
}

expect 0 --help
expect 0 -h
expect 2
expect 2 -Z
expect 2 $'-\001'
expect 2 -o
expect 2 -o "" -- "$true_path"
expect 2 -r "" -- "$true_path"
expect 2 -d "" -- "$true_path"
expect 2 -t "" -- "$true_path"
expect 2 -p "" -- "$true_path"
expect 0 -C -o "$work/capture-only" -- "$true_path"
test -e "$work/capture-only/resources.log"
test -e "$work/capture-only/calls.log"
test ! -e "$work/capture-only/correlated-report.txt"
grep -q 'analysis=deferred' "$work/capture-only/receipt.txt"
expect 1 -C -o "$work/capture-failure" -- "$false_path"

cat >"$work/helper" <<'HELPER'
#!/usr/bin/env bash
if [ "${1:-}" = "-j" ]; then
  printf '{"schema":"p101-resource-tracker-findings-v3","records":0,"fd_leaks":0,"allocation_leaks":0,"bad_releases":0,"exec_inheritances":0,"generic_resource_leaks":0,"generic_bad_releases":0,"malformed":0,"bad_version":0,"refused":0,"log_health":{"complete":true}}\n'
fi
if [ "${1:-}" = "-b" ]; then
  : >"$2/correlated-report.txt"
  : >"$2/correlated-report.json"
  : >"$2/resource-lifetimes.md"
  printf '{"schema":"p101-run-model-v1","event_schema":"p101-tool-event-format-v5","nodes":[],"edges":[]}\n' >"$2/run-model.json"
fi
exit "${P101_TEST_HELPER_STATUS:-0}"
HELPER
chmod +x "$work/helper"

run_observe() {
  report=$1
  shift
  expect "$@" -o "$report" -r "$work/helper" -d "$work/helper" -t "$work/helper" -p "$work/helper" -- "$true_path"
}

run_observe "$work/clean" 0
test -s "$work/clean/summary.txt"
test -s "$work/clean/receipt.txt"
run_observe "$work/clean" 2

P101_TEST_HELPER_STATUS=1 run_observe "$work/findings" 1
P101_TEST_HELPER_STATUS=2 run_observe "$work/trouble" 2

cat >"$work/incomplete-helper" <<'HELPER'
#!/usr/bin/env bash
if [ "${1:-}" = "-j" ]; then printf '{}\n'; fi
exit 0
HELPER
chmod +x "$work/incomplete-helper"
expect 2 -o "$work/incomplete" -r "$work/incomplete-helper" -d "$work/incomplete-helper" -t "$work/incomplete-helper" -p "$work/incomplete-helper" -- "$true_path"

cat >"$work/unhealthy-helper" <<'HELPER'
#!/usr/bin/env bash
if [ "${1:-}" = "-j" ]; then
  printf '{"schema":"p101-resource-tracker-findings-v3","records":0,"fd_leaks":0,"allocation_leaks":0,"bad_releases":0,"exec_inheritances":0,"generic_resource_leaks":0,"generic_bad_releases":0,"malformed":0,"bad_version":0,"refused":0,"log_health":{"complete":false}}\n'
fi
exit 0
HELPER
chmod +x "$work/unhealthy-helper"
expect 2 -o "$work/unhealthy" -r "$work/unhealthy-helper" -d "$work/unhealthy-helper" -t "$work/unhealthy-helper" -p "$work/unhealthy-helper" -- "$true_path"

expect 1 -o "$work/command-fail" -r "$work/helper" -d "$work/helper" -t "$work/helper" -p "$work/helper" -- "$false_path"
expect 1 -o "$work/exec-fail" -r "$work/helper" -d "$work/helper" -t "$work/helper" -p "$work/helper" -- /definitely/missing
expect 2 -o "$work/tool-exec-fail" -r /definitely/missing -d "$work/helper" -t "$work/helper" -p "$work/helper" -- "$true_path"

expect 0 -v -A -R -o "$work/verbose" -r "$work/helper" -d "$work/helper" -t "$work/helper" -p "$work/helper" -- "$true_path"

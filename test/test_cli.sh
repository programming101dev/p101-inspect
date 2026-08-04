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
expect 2 -r retired -- "$true_path"

expect 0 -o "$work/capture" -- "$true_path"
test -e "$work/capture/resources.log"
test -e "$work/capture/calls.log"
test ! -e "$work/capture/correlated-report.txt"
grep -q 'analysis=deferred' "$work/capture/receipt.txt"
grep -q '"schema":"p101-tool-run-receipt-v4"' "$work/capture/tool-receipt.json"
grep -q '"input":{"schema":"p101-run-receipt-v1"' "$work/capture/tool-receipt.json"
grep -q '"policy":{"schema":"p101-observe-capture-policy-v1"' "$work/capture/tool-receipt.json"
grep -q '"receipt_digest":{"algorithm":"fnv1a64-semantic-v1"' "$work/capture/tool-receipt.json"

expect 1 -o "$work/command-fail" -- "$false_path"
expect 1 -o "$work/exec-fail" -- /definitely/missing
expect 0 -v -A -R -o "$work/verbose" -- "$true_path"

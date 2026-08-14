#!/usr/bin/env bash
set -euo pipefail

tool=$1
fixtures=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-inspect-native-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

expect() {
  wanted=$1
  shift
  set +e
  "$tool" "$@" >"$work/stdout" 2>"$work/stderr"
  got=$?
  set -e
  if [ "$got" -ne "$wanted" ]; then
    printf 'expected %s, got %s: %s\n' "$wanted" "$got" "$*" >&2
    cat "$work/stdout" >&2
    cat "$work/stderr" >&2
    return 1
  fi
}

mkdir -p "$work/capture"
cp "$fixtures/model-resources.log" "$work/capture/resources.log"
cp "$fixtures/model-calls.log" "$work/capture/calls.log"
: >"$work/capture/stderr.txt"

expect 0 --help
expect 2 --p101-deliberately-invalid-option
expect 0 lesson P101-API-001
grep -q 'P101-LESSON-API-COMPATIBILITY' "$work/stdout"
expect 1 lesson P101-NOT-REGISTERED
grep -q 'finding has no registered lesson' "$work/stderr"
expect 0 analyze --force -o "$work/analysis" "$work/capture"
test -s "$work/analysis/run-model.json"
test -s "$work/analysis/resource-report.json"
test -s "$work/analysis/correlated-report.txt"
test -s "$work/analysis/analysis-receipt.json"

expect 0 model verify "$work/analysis"
cat >"$work/unsatisfied.expectations" <<'EOF'
p101-expectations-v1
result=findings
EOF
expect 1 model verify -e "$work/unsatisfied.expectations" "$work/analysis"
grep -q 'P101-EXPECT-001' "$work/stderr"
expect 0 model compare "$work/analysis" "$work/analysis"
expect 0 model check "$work/analysis" --rules resource-clean
expect 0 view resource "$work/analysis"
expect 0 view -d:json resource "$work/analysis"
grep -q '"schema"' "$work/stdout"
expect 0 view -d:human,json report "$work/analysis"
grep -q '"schema"' "$work/stdout"
grep -q 'p101 correlated runtime report' "$work/stderr"
expect 0 view -s report "$work/analysis"
expect 0 view -m report "$work/analysis"
expect 0 view sync "$work/analysis"
expect 0 view trace "$work/analysis"
expect 0 view report "$work/analysis"
expect 0 interleaving "$work/analysis"

cp -R "$work/analysis" "$work/tampered"
printf '\ntampered\n' >>"$work/tampered/correlated-report.json"
expect 2 model verify "$work/tampered"

mkdir -p "$work/sync-capture"
: >"$work/sync-capture/stderr.txt"
cat >"$work/sync-capture/resources.log" <<'EOF'
P101RESOURCE	5	sync-run	10	1	1	100	100	ACQUIRE	pthread-mutex-held	A	-	0	thread=1	1	thread_one	threads.c
P101RESOURCE	5	sync-run	10	1	2	110	110	ACQUIRE	pthread-mutex-wait	B	-	0	thread=1	2	thread_one	threads.c
P101RESOURCE	5	sync-run	10	1	3	120	120	RELEASE	pthread-mutex-wait	B	-	0	thread=1	3	thread_one	threads.c
P101RESOURCE	5	sync-run	10	1	4	130	130	RELEASE	pthread-mutex-held	A	-	0	thread=1	4	thread_one	threads.c
P101RESOURCE	5	sync-run	10	2	5	140	140	ACQUIRE	pthread-mutex-held	B	-	0	thread=2	5	thread_two	threads.c
P101RESOURCE	5	sync-run	10	2	6	150	150	ACQUIRE	pthread-mutex-wait	A	-	0	thread=2	6	thread_two	threads.c
P101RESOURCE	5	sync-run	10	2	7	160	160	RELEASE	pthread-mutex-wait	A	-	0	thread=2	7	thread_two	threads.c
P101RESOURCE	5	sync-run	10	2	8	170	170	RELEASE	pthread-mutex-held	B	-	0	thread=2	8	thread_two	threads.c
P101COMPLETE	5	sync-run	10	1	9	180	180	4	0	0
P101COMPLETE	5	sync-run	10	2	10	190	190	4	0	0
EOF
cat >"$work/sync-capture/calls.log" <<'EOF'
P101COMPLETE	5	sync-run	10	1	9	180	180	0	0	0
P101COMPLETE	5	sync-run	10	2	10	190	190	0	0	0
EOF
expect 0 analyze --force -o "$work/sync-analysis" "$work/sync-capture"
expect 1 interleaving "$work/sync-analysis"
grep -q 'P101-SYNC-002' "$work/stdout"

mkdir -p "$work/leak-capture"
: >"$work/leak-capture/stderr.txt"
cat >"$work/leak-capture/resources.log" <<'EOF'
P101RESOURCE	5	leak-run	10	1	1	100	100	ACQUIRE	allocation	0x101	-	16	-	1	allocate	memory.c
P101COMPLETE	5	leak-run	10	1	2	110	110	1	0	0
EOF
cat >"$work/leak-capture/calls.log" <<'EOF'
P101COMPLETE	5	leak-run	10	1	2	110	110	0	0	0
EOF
expect 1 analyze --force -o "$work/leak-analysis" "$work/leak-capture"
expect 1 view resource "$work/leak-analysis"
expect 0 view sync "$work/leak-analysis"

printf 'p101-inspect native CLI regression: PASS\n'

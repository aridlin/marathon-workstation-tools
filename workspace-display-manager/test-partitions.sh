#!/usr/bin/env bash
set -euo pipefail

binary=${1:-./workspace-display-manager}

expect() {
  local count=$1 main=$2 expected=$3 actual
  actual=$($binary --print-plan "$count" "$main")
  if [[ "$actual" != "$expected" ]]; then
    printf 'partition test failed for count=%s main=%s\nexpected:\n%s\nactual:\n%s\n' \
      "$count" "$main" "$expected" "$actual" >&2
    exit 1
  fi
}

expect 1 0 $'monitor 1: 1-10 main'
expect 2 0 $'monitor 1: 1-5 main\nmonitor 2: 6-10'
expect 3 0 $'monitor 1: 1-4 main\nmonitor 2: 5-7\nmonitor 3: 8-10'
expect 3 1 $'monitor 1: 1-3\nmonitor 2: 4-7 main\nmonitor 3: 8-10'
expect 4 0 $'monitor 1: 1-3 main\nmonitor 2: 4-6\nmonitor 3: 7-8\nmonitor 4: 9-10'
expect 5 0 $'monitor 1: 1-2 main\nmonitor 2: 3-4\nmonitor 3: 5-6\nmonitor 4: 7-8\nmonitor 5: 9-10'

tsv=$($binary --print-plan-tsv 2 0)
[[ $tsv == $'0\t1\t5\n1\t6\t10' ]]

printf 'all partition tests passed\n'

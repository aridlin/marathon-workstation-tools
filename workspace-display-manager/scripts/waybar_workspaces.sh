#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
grid_current

TEXT=''
for i in {1..10}; do
  if (( i == GRID_MAIN )); then
    TEXT+="<span foreground='#00130b' background='#3f8f61'> $i </span>"
  else
    TEXT+="<span foreground='#b5d2bc' background='#002d18'> $i </span>"
  fi
done
TEXT+='\n'
for i in {1..10}; do
  grid_layout_for_sub "$i" || continue
  if (( i == GRID_SUB )); then
    TEXT+="<span foreground='#00130b' background='#4fae78'> $i </span>"
  elif (( GRID_MONITOR_IS_MAIN == 1 )); then
    TEXT+="<span foreground='#9bd7aa' background='#123b27'> $i </span>"
  else
    TEXT+="<span foreground='#76ac84' background='#001d10'> $i </span>"
  fi
done
grid_layout_for_sub "$GRID_SUB" || GRID_MONITOR='unknown'
printf '{"text":"%s","tooltip":"main:%d sub:%d · %s"}\n' \
  "$TEXT" "$GRID_MAIN" "$GRID_SUB" "$GRID_MONITOR"

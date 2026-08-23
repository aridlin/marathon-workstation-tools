#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/workspace_grid_lib.sh"
grid_current
monitor=$(grid_focused_monitor) || exit 0
grid_layout_for_monitor "$monitor" || exit 0
if (( GRID_SUB >= GRID_RANGE_FIRST && GRID_SUB <= GRID_RANGE_LAST )); then
  grid_store_main "$GRID_MAIN"
  grid_store_sub "$GRID_MAIN" "$monitor" "$GRID_SUB"
fi

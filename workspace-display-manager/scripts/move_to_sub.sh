#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
SUB=${1:-}
grid_valid_axis "$SUB" || exit 2
grid_current
grid_layout_for_sub "$SUB" || exit 1
grid_store_sub "$GRID_MAIN" "$GRID_MONITOR" "$SUB"
hyprctl dispatch movetoworkspace "$(grid_target "$GRID_MAIN" "$SUB")" >/dev/null

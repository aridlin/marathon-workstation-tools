#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/workspace_grid_lib.sh"
MAIN=${1:-}
SUB=${2:-}
grid_valid_axis "$MAIN" && grid_valid_axis "$SUB" || exit 2
grid_layout_for_sub "$SUB" || exit 1
owner=$GRID_MONITOR
grid_store_sub "$MAIN" "$owner" "$SUB"
grid_switch_main "$MAIN"
hyprctl dispatch focusmonitor "$owner" >/dev/null
hyprctl dispatch workspace "$(grid_target "$MAIN" "$SUB")" >/dev/null

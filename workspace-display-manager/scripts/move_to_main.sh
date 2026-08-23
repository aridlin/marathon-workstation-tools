#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
MAIN=${1:-}
grid_valid_axis "$MAIN" || exit 2
monitor=$(grid_focused_monitor) || exit 1
grid_layout_for_monitor "$monitor" || exit 1
sub=$(grid_saved_sub "$MAIN" "$monitor" "$GRID_RANGE_FIRST" "$GRID_RANGE_LAST")
grid_store_sub "$MAIN" "$monitor" "$sub"
hyprctl dispatch movetoworkspace "$(grid_target "$MAIN" "$sub")" >/dev/null
grid_switch_main "$MAIN"

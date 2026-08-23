#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
grid_current
monitor=$(grid_focused_monitor) || exit 1
grid_layout_for_monitor "$monitor" || exit 1
sub=$((GRID_SUB - 1))
(( sub < GRID_RANGE_FIRST || sub > GRID_RANGE_LAST )) && sub=$GRID_RANGE_LAST
exec "$(dirname "$0")/goto_workspace.sh" "$GRID_MAIN" "$sub"

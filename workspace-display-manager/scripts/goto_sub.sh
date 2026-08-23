#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
SUB=${1:-}
grid_valid_axis "$SUB" || exit 2
grid_current
exec "$(dirname "$0")/goto_workspace.sh" "$GRID_MAIN" "$SUB"

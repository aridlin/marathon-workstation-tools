#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
grid_current
exec "$(dirname "$0")/set_main.sh" "$(( (GRID_MAIN + 8) % 10 + 1 ))"

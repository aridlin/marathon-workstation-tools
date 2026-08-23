#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/workspace_grid_lib.sh"
MAIN=${1:-}
grid_valid_axis "$MAIN" || exit 2
grid_switch_main "$MAIN"

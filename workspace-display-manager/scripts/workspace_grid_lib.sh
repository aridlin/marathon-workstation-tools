#!/usr/bin/env bash

GRID_CONFIG_DIR=${XDG_CONFIG_HOME:-$HOME/.config}/hypr
GRID_CACHE_DIR=${XDG_CACHE_HOME:-$HOME/.cache}/hypr
GRID_LAYOUT_FILE=$GRID_CACHE_DIR/workspace-display-layout.tsv

grid_valid_axis() {
  [[ ${1:-} =~ ^([1-9]|10)$ ]]
}

grid_saved_axis() {
  local file=$1 fallback=$2 value
  if [[ -r $file ]]; then
    value=$(<"$file")
  else
    value=$fallback
  fi
  grid_valid_axis "$value" || value=$fallback
  printf '%s\n' "$value"
}

grid_current() {
  local id
  id=$(hyprctl activeworkspace -j 2>/dev/null | jq -er '.id') || id=''
  if [[ $id =~ ^[0-9]+$ ]] && (( id >= 11 && id <= 110 )); then
    GRID_MAIN=$(( (id - 1) / 10 ))
    GRID_SUB=$(( (id - 1) % 10 + 1 ))
  else
    GRID_MAIN=$(grid_saved_axis /tmp/hypr_main_workspace 1)
    GRID_SUB=1
  fi
}

grid_target() {
  printf '%s\n' $(( $1 * 10 + $2 ))
}

grid_monitor_key() {
  printf '%s' "$1" | sha256sum | cut -c1-16
}

grid_ensure_layout() {
  if [[ ! -s $GRID_LAYOUT_FILE ]]; then
    "$GRID_CONFIG_DIR/workspace_monitor_apply.sh" >/dev/null 2>&1 || return 1
  fi
}

grid_layout_for_sub() {
  local wanted=$1 index monitor first last is_main
  grid_ensure_layout || return 1
  while IFS=$'\t' read -r index monitor first last is_main; do
    [[ $index == \#* || -z $monitor ]] && continue
    if (( wanted >= first && wanted <= last )); then
      GRID_MONITOR=$monitor
      GRID_RANGE_FIRST=$first
      GRID_RANGE_LAST=$last
      GRID_MONITOR_IS_MAIN=$is_main
      return 0
    fi
  done < "$GRID_LAYOUT_FILE"
  return 1
}

grid_layout_for_monitor() {
  local wanted=$1 index monitor first last is_main
  grid_ensure_layout || return 1
  while IFS=$'\t' read -r index monitor first last is_main; do
    [[ $index == \#* || -z $monitor ]] && continue
    if [[ $monitor == "$wanted" ]]; then
      GRID_MONITOR=$monitor
      GRID_RANGE_FIRST=$first
      GRID_RANGE_LAST=$last
      GRID_MONITOR_IS_MAIN=$is_main
      return 0
    fi
  done < "$GRID_LAYOUT_FILE"
  return 1
}

grid_focused_monitor() {
  hyprctl monitors -j 2>/dev/null | jq -er '.[] | select(.focused == true) | .name' | head -n1
}

grid_main_monitor() {
  local index monitor first last is_main
  grid_ensure_layout || return 1
  while IFS=$'\t' read -r index monitor first last is_main; do
    [[ $index == \#* || -z $monitor ]] && continue
    if [[ $is_main == 1 ]]; then
      printf '%s\n' "$monitor"
      return 0
    fi
  done < "$GRID_LAYOUT_FILE"
  return 1
}

grid_saved_sub() {
  local main=$1 monitor=$2 first=$3 last=$4 key file value legacy
  key=$(grid_monitor_key "$monitor")
  file="/tmp/hypr_sub_${main}_${key}"
  value=''
  [[ -r $file ]] && value=$(<"$file")
  if ! grid_valid_axis "$value" || (( value < first || value > last )); then
    legacy=$(grid_saved_axis "/tmp/hypr_sub_${main}" "$first")
    if (( legacy >= first && legacy <= last )); then value=$legacy; else value=$first; fi
  fi
  printf '%s\n' "$value"
}

grid_store_sub() {
  local main=$1 monitor=$2 sub=$3 key
  key=$(grid_monitor_key "$monitor")
  printf '%s\n' "$sub" > "/tmp/hypr_sub_${main}_${key}"
  printf '%s\n' "$sub" > "/tmp/hypr_sub_${main}"
}

grid_store_main() {
  printf '%s\n' "$1" > /tmp/hypr_main_workspace
}

grid_switch_main() {
  local main=$1 index monitor first last is_main sub focus_target=''
  grid_valid_axis "$main" || return 2
  grid_ensure_layout || return 1
  grid_store_main "$main"
  while IFS=$'\t' read -r index monitor first last is_main; do
    [[ $index == \#* || -z $monitor ]] && continue
    sub=$(grid_saved_sub "$main" "$monitor" "$first" "$last")
    hyprctl dispatch focusmonitor "$monitor" >/dev/null
    hyprctl dispatch workspace "$(grid_target "$main" "$sub")" >/dev/null
    [[ $is_main == 1 ]] && focus_target=$monitor
  done < "$GRID_LAYOUT_FILE"
  [[ -n $focus_target ]] && hyprctl dispatch focusmonitor "$focus_target" >/dev/null
}

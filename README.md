# Marathon Workstation Tools

A native C++ toolkit for a fast, portable Hyprland workstation: a polished
command reference, a warm 10x10 workspace overview, and a monitor-aware 2D
workspace router with a graphical display-order editor and hotplug daemon.

The project grew out of a real external-SSD Arch workstation that must remain
usable when moved between laptops, desktops, docks, projectors, and one-to-five
monitor setups. The tools favor predictable state, user-level installation,
fast startup, and graceful behavior when optional desktop components are absent.

## Included tools

| Tool | Purpose | Implementation |
| --- | --- | --- |
| `ahelp` | Attractive terminal-sized command reference split into standard/external, custom-built, and live-discovery sections | C++20, ANSI truecolor, Unicode/Nerd Font |
| `workspace-field` | Floating 10x10 Hyprland workspace overview with application icons, badges, keyboard navigation, and fast warm activation | C++20, GTK3, Cairo, JSON-GLib |
| `workspace-display-manager` | GUI for display order/main-display selection, deterministic workspace partitioning, generated Hyprland rules, and hotplug repair | C++20, GTK3, JSON-GLib, Hyprland socket2 IPC |
| workspace scripts | Main/sub navigation, window movement, per-monitor memory, Waybar data, and generated workspace ownership | Bash, `hyprctl`, `jq` |

## The 2D workspace model

The workspace space is a 10x10 grid:

- main axis: `1..10`
- sub axis: `1..10`
- Hyprland workspace ID: `main * 10 + sub`

That means main row 1 is `11..20`, main row 2 is `21..30`, and main row
10 is `101..110`.

The main axis represents a task/context. The sub axis represents positions
inside that context. Switching the main axis updates every connected monitor
to its remembered sub-workspace for that row. Direct sub navigation focuses
the monitor that owns that slice.

### Monitor partitioning

Every main row independently partitions its ten sub-workspaces into contiguous,
balanced slices. Display order comes from the GUI. The selected main display
receives the first remainder slot when ten does not divide evenly.

| Displays | Sub-workspace ownership in every main row |
| ---: | --- |
| 1 | `1-10` |
| 2 | `1-5`, `6-10` |
| 3, display 1 main | `1-4`, `5-7`, `8-10` |
| 3, display 2 main | `1-3`, `4-7`, `8-10` |
| 4, display 1 main | `1-3`, `4-6`, `7-8`, `9-10` |
| 5 | `1-2`, `3-4`, `5-6`, `7-8`, `9-10` |

For two displays this produces `11-15` and `16-20` in row 1, then
`21-25` and `26-30` in row 2. For five displays, row 1 becomes `11-12`,
`13-14`, `15-16`, `17-18`, and `19-20`.

`workspace-display-manager --print-plan COUNT MAIN_INDEX` exposes the pure
partition engine without Hyprland. `make test` validates all supported layouts.

## Workspace Field

`workspace-field` is the visual front end for the full grid.

- Shows all 100 workspace coordinates at once.
- Reads windows from `hyprctl clients -j`.
- Resolves application icons through the installed desktop icon theme.
- Combines different apps as vertical icon slices.
- Adds count badges for repeated apps.
- Shows up to three titles from the selected workspace.
- Supports pointer selection and wrapping arrow-key navigation.
- `Enter` or `Space` switches; `Escape` closes.
- Running the command again toggles the existing popup closed.
- `--daemon` keeps a hidden `GtkApplication` warm, avoiding GTK cold-start cost.

The selected workspace is routed through `goto_workspace.sh`, so clicks obey
the same monitor ownership and per-monitor memory as keyboard navigation.

## Display manager and hotplug daemon

Launch **Workspace Display Layout** from Walker, KRunner, or another desktop
launcher. It deliberately has no mandatory keybinding.

The GUI:

1. reads active Hyprland outputs;
2. lets you move displays up/down into the desired logical order;
3. lets you select the main display;
4. previews each display's sub-workspace range;
5. writes `~/.config/hypr/workspace-displays.json`;
6. applies the layout immediately.

The `--daemon` mode connects directly to Hyprland's socket2 event stream. On
`monitoradded` or `monitorremoved`, it debounces the event and reapplies the
layout. No `socat` process is required.

The apply path generates 100 workspace rules in
`~/.config/hypr/workspace-monitors.generated.conf`, moves existing grid
workspaces to their correct owners, preserves the current main row, and avoids
reloads when the generated file is unchanged.

## ahelp

`ahelp` is intentionally organized first and dynamic second. Its stable,
hand-ordered reference separates installed external applications from
custom-built workstation utilities:

- **Standard / external commands** — AI/development/data, desktop/capture,
  system/connectivity, and games/compatibility.
- **Custom-built utilities** — terminal/content, workspaces/Wayland,
  files/packages/processes, and networking.

Entries are ordered by practical usefulness/frequency, not alphabet. Each has a
contextual Nerd Font icon and a human description. One-off transfer/notifier
jobs, including `proliant-*` and `rat-quest-*`, redundant wrappers, and
non-command directory entries are deliberately excluded from both curated and
dynamic output. The following sections are explicitly labeled dynamic and contain:

- aliases sourced from `~/.zshrc`;
- user functions whose source is `~/.zshrc`;
- executable files discovered in `~/.local/bin` or `~/bin` that are not already
  in the curated table.

The shell scan runs in an isolated non-interactive Zsh child process. The fixed
categories therefore remain deliberate while new commands are still visible.

Usage:

```console
ahelp                  # direct, non-interactive output
ahelp workspace        # filter every section
ahelp -i               # opt-in interactive pager
ahelp --plain          # no ANSI color, suitable for scripts/logs
```

The renderer adapts to terminal width:

- every command is a separate rounded, note-like blob;
- name and content determine each card's preferred width and wrapped height;
- the live width greedily packs one, two, three, or more varied cards per row;
- incomplete rows stay compact and centered instead of growing empty cells;
- six restrained truecolor palettes visually separate adjacent cards;
- command names are bold and brighter than their card borders;
- direct output reverses category order to place primary sections nearest the
  prompt, while `-i` keeps natural forward order for paging;
- Private Use Area glyphs are marked printable for the optional `less` pager,
  preventing `<U+F...>` escapes.

The fixed reference is entirely data-driven from
`~/.config/ahelp/commands.tsv`. Its six tab-separated fields are group,
subcategory, subcategory icon, command, command icon, and description; row order
is display priority. `make install` installs the repository catalog and
`AHELP_CONFIG=/path/to/commands.tsv` selects a temporary alternative. Add
one-off names to `kIgnoredDynamicCommands` when they should not reappear through
live discovery, or use `kIgnoredDynamicPrefixes` for a whole temporary helper
family. Dynamic discoveries never mutate the catalog.

## Dependencies

### Arch Linux

```bash
sudo pacman -S --needed base-devel pkgconf gtk3 json-glib jq zsh less
```

Runtime workspace integration additionally needs Hyprland. Walker/KRunner,
Waybar, and a Nerd Font are optional but recommended.

### Debian/Ubuntu build host

```bash
sudo apt-get install build-essential pkg-config libgtk-3-dev \
  libjson-glib-dev jq zsh less
```

The workspace tools themselves still require a Hyprland runtime.

## Build and test

```bash
git clone https://github.com/aridlin/marathon-workstation-tools.git
cd marathon-workstation-tools
make
make test
```

`make test` builds all three C++ binaries, exercises filtered `ahelp` output,
and validates the 1-5-monitor partition engine including main-display remainder
assignment.

Build an individual component with, for example:

```bash
make -C workspace-field
make -C workspace-display-manager test
make -C ahelp test
```

## User installation

```bash
make install
```

This installs only user-owned files:

- binaries under `~/.local/bin`;
- the editable ahelp catalog under `~/.config/ahelp`;
- workspace scripts under `~/.config/hypr`;
- the display-manager desktop entry under `~/.local/share/applications`;
- its scalable icon under `~/.local/share/icons/hicolor/scalable/apps`.

No root access is used and the installer does not overwrite `hyprland.conf`.
Merge [`examples/hyprland.conf`](examples/hyprland.conf) into your own config,
then run:

```bash
~/.config/hypr/workspace_monitor_apply.sh
hyprctl reload
```

The generated file must be sourced before grid navigation is used:

```ini
source = ~/.config/hypr/workspace-monitors.generated.conf
```

## Suggested controls

The full example config provides these controls:

| Binding | Action |
| --- | --- |
| `Super+Q` | Toggle Workspace Field |
| `Super+1..0` | Switch main row on all monitors |
| `Super+Ctrl+1..0` | Switch sub-workspace and focus its owner monitor |
| `Super+Shift+1..0` | Move active window to another main row |
| `Super+Shift+Ctrl+1..0` | Move active window to another sub-workspace |

`cycle_main*.sh` and `cycle_sub*.sh` are provided for wheel/gesture bindings.
The sub-axis cycle stays inside the focused monitor's assigned slice.

## Waybar integration

`waybar_workspaces.sh` emits workspace state for a custom module. It uses the
same cached ownership and remembered axes as the GUI and navigation scripts.
Call it from a Waybar custom module rather than duplicating workspace math in
the bar configuration.

## Files and state

| Path | Meaning |
| --- | --- |
| `~/.config/hypr/workspace-displays.json` | Saved display order and selected main output |
| `~/.config/hypr/workspace-monitors.generated.conf` | Generated Hyprland workspace-to-monitor rules |
| `~/.cache/hypr/workspace-display-layout.tsv` | Current ordered output/range cache shared by scripts |
| `/tmp/hypr_main_workspace` | Remembered main axis for the current boot/session |
| `/tmp/hypr_sub_MAIN_HASH` | Remembered sub axis per main row and monitor |

Monitor names are hashed only for safe temporary filenames; the actual monitor
names remain visible in the JSON and TSV configuration files.

## Architecture

```text
Hyprland socket2 hotplug events
              │
              ▼
workspace-display-manager --daemon
              │
              ▼
workspace_monitor_apply.sh ──► generated workspace rules
              │
              ├──► ownership TSV cache
              └──► repair/move existing workspaces

keyboard scripts ─┐
workspace-field ──┼──► goto_workspace.sh ──► hyprctl dispatch
Waybar module ────┘
```

The C++ partition engine is the single source of truth for range allocation.
The shell apply layer consumes its TSV output instead of reimplementing the
arithmetic.

## Troubleshooting

### The display-layout app is missing from Walker

Confirm that the desktop entry exists and refresh the application index:

```bash
test -r ~/.local/share/applications/workspace-display-manager.desktop
gtk-update-icon-cache ~/.local/share/icons/hicolor 2>/dev/null || true
```

### Workspace commands do nothing

Check that `jq` is installed, Hyprland's environment is available, the scripts
are executable, and the generated config is sourced:

```bash
hyprctl monitors -j | jq '.[].name'
~/.local/bin/workspace-display-manager --apply
hyprctl configerrors
```

### `ahelp -i` shows escaped Nerd Font codepoints

The program supplies `LESSUTFCHARDEF` for all Unicode Private Use Areas. If the
glyph is blank rather than escaped, install and select a current Nerd Font in
the terminal emulator.

### More than five displays

The first five ordered connected outputs participate in the 10-slot partition.
This is intentional: ten sub-workspaces cannot give every monitor a useful
contiguous slice beyond five outputs.

## Safety and portability

- No privileged daemon is installed.
- Configuration and state remain inside the user's home/runtime directories.
- Generated files are written through temporary files and renamed into place.
- Apply operations use a non-blocking lock to prevent hotplug races.
- The monitor manager ignores disconnected saved outputs and deterministically
  appends newly discovered outputs by physical position/name.
- Existing workspaces are repaired after ownership changes.
- All normal CLI output ends with a newline.

## License

MIT. See [`LICENSE`](LICENSE).

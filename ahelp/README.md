# ahelp

Native C++ command index for this workstation. Its primary table is a stable,
hand-curated personal command reference with individual contextual Nerd Font
icons. Separate live-discovery tables source `.zshrc` in an isolated
non-interactive Zsh process and list aliases, user functions, and unlisted
executables found in `~/.local/bin` and `~/bin`.

The curated table stays deliberate and consistently ordered. Newly added
aliases, functions, and user-bin executables still appear below it without
silently changing the primary list.

The output is a terminal-width-aware Unicode/Nerd Font table with true-color
panels, alternating rows, and cell wrapping. It prints directly by default.

Use `ahelp QUERY` to filter, `ahelp -i` for interactive pager navigation, and
`ahelp --plain` for scripting. The optional pager is configured to display
Nerd Font private-use glyphs instead of escaping them as `<U+...>`.

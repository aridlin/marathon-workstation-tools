# ahelp

Native C++ command index for this workstation. The fixed reference is divided
into two explicit top-level groups:

- **Standard / external commands** for installed applications such as Codex,
  Firefox, Flameshot, Ghostty, Syncthing, and development tools.
- **Custom-built utilities** for personal tools such as `wemote`, `overcalc`,
  `mdunicode`, `fckmpeg`, the workspace tools, and reusable workstation helpers.

Both groups have focused subcategories for development, desktop, system,
terminal/content, Wayland/workspaces, file/process work, and homelab tasks.
The fixed list stays deliberate and consistently ordered. One-off job helpers,
redundant package-manager wrappers, and directory noise are explicitly excluded.

Separate dynamic-discovery tables source `.zshrc` in an isolated non-interactive
Zsh process and list aliases, user functions, and useful unlisted executables
found in `~/.local/bin` and `~/bin`. Newly added commands therefore remain
visible without silently changing the curated sections.

The output is a terminal-width-aware Unicode/Nerd Font table with true-color
panels, alternating rows, and cell wrapping. It prints directly by default.

Use `ahelp QUERY` to filter, `ahelp -i` for interactive pager navigation, and
`ahelp --plain` for scripting. The optional pager is configured to display
Nerd Font private-use glyphs instead of escaping them as `<U+...>`.

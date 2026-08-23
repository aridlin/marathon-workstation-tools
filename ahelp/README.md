# ahelp

Native C++ command index for this workstation. The fixed reference is divided
into two explicit top-level groups:

- **Standard / external commands** for installed applications such as Codex,
  Firefox, Flameshot, Ghostty, Syncthing, and development tools.
- **Custom-built utilities** for personal tools such as `wemote`, `overcalc`,
  `mdunicode`, `fckmpeg`, the workspace tools, and reusable workstation helpers.

Both groups have focused subcategories for development, desktop, system,
terminal/content, Wayland/workspaces, file/process work, and networking. Commands
inside them are ordered by likely usefulness and frequency rather than alphabet.
The fixed list stays deliberate: one-off transfer/notifier jobs (including all
`proliant-*` and `rat-quest-*` helpers), redundant package-manager wrappers, and
directory noise are explicitly excluded from both fixed and dynamic output.

Separate dynamic-discovery tables source `.zshrc` in an isolated non-interactive
Zsh process and list aliases, user functions, and useful unlisted executables
found in `~/.local/bin` and `~/bin`. Newly added commands therefore remain
visible without silently changing the curated sections.

The output is a terminal-width-aware Unicode/Nerd Font note board. Every command
is a separate rounded blob whose preferred width comes from its name and content.
The renderer greedily packs one, two, three, or more varied cards into each row,
keeps incomplete rows compact and centered, and lets descriptions determine card
height. Six restrained accent/background palettes make adjacent notes distinct
in color terminals. It prints directly by default.

Use `ahelp QUERY` to filter, `ahelp -i` for interactive pager navigation, and
`ahelp --plain` for scripting. The optional pager is configured to display
Nerd Font private-use glyphs instead of escaping them as `<U+...>`.

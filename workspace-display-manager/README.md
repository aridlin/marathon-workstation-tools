# Workspace Display Layout

GTK3 configurator and hot-plug daemon for the 10x10 Hyprland workspace field.

Each main row has ten sub-workspaces. One to five ordered displays receive
contiguous, balanced slices of those ten sub-workspaces. The selected main
display receives the first remainder slot when ten is not evenly divisible.

Examples:

- 2 displays: `1-5`, `6-10`
- 3 displays: `1-4`, `5-7`, `8-10` when display 1 is main
- 5 displays: `1-2`, `3-4`, `5-6`, `7-8`, `9-10`

The split repeats in every main row, so the two-display split for main row 2 is
`21-25` and `26-30`.

Run `make test` to validate the partition engine and `make install` to install
the binary. The desktop entry is intended for Walker; there is no key binding.

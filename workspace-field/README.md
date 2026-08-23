# Workspace Field

A floating 10×10 overview for the Hyprland workspace coordinates in this setup.

- Columns are main workspaces 1–10.
- Rows are sub workspaces 1–10.
- A click routes `main*10+sub` through the monitor-aware workspace dispatcher
  and closes the popup.
- App icons are read from `hyprctl clients -j` and the installed desktop icon theme.
- Repeated apps get a count badge; mixed apps are combined as vertical icon slices.
- Running the binary again toggles the existing popup closed.
- Arrow keys wrap around the field; `Enter` or `Space` teleports and `Escape` closes.
- The selection glides in pixel-space (including edge wraps), and the header shows up to three titles from the selected workspace.

Build and install with `make install`. The live Hyprland config binds it to `Super+Q`.
Hyprland starts a hidden `--daemon` instance so opening the field does not pay GTK cold-start costs.
The `Super+Q` binding activates that warm instance directly over D-Bus; it mapped in roughly 48 ms during live testing.

# Roadmap

## Shipped

Highlight → optional snap → crop → placeable source. Windows installer to `%ProgramData%\obs-studio\plugins\`, portable zip, launch-time GitHub update check.

- Plugin bootstrap + CI
- Sample any source or scene, masked draw
- Cutout editor (paint, line, fill, Magic Select, Snap Edges)
- Works on a vertical canvas and on a vanilla OBS scene

## Auto-hide (this branch)

[#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20)

- Auto-hide checkbox, **off by default**
- Whole-instance hide when the painted outline is gone
- **One shared presence cycle** for all visible auto-hide masks (grouped by sampled source)
- Fade (0 = instant) and Match sliders; hysteresis so 0 ms does not strobe

Per-island hide inside one mask is a follow-up.

## Later

- macOS / Linux builds and installers

## Not doing

- Motion-based UI discovery
- Per-game mask libraries
- Full-frame autodetect with no highlight
- Auto-layout on the canvas
- Auto-resize of islands when HUD chrome grows/shrinks
- Shipping PNG files as the way to set up a game
- Split into N sources (closed; use one HUD Mask per element)

## Issues

| Status | Issues |
| --- | --- |
| Next | [#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20) auto-hide |

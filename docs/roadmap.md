# Roadmap

## Shipped

Highlight → optional snap → crop → placeable source. Windows installer to `%ProgramData%\obs-studio\plugins\`, portable zip, launch-time GitHub update check.

- Plugin bootstrap + CI
- Sample any source or scene, masked draw
- Cutout editor (paint, line, fill, Magic Select, Snap Edges)
- Works on a vertical canvas and on a vanilla OBS scene

## Next — optional auto-hide

[#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20)

- Auto-hide checkbox, **off by default**
- Whole-instance hide when the element is gone
- **One shared presence cycle** for all visible auto-hide masks (grouped by sampled source)
- Hysteresis + fade; signatures ignore changing fill

Per-island hide inside one mask is a follow-up, not the first slice.

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

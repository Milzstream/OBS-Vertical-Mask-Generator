# Roadmap

## MVP — cut-out

Highlight → cleanup → crop → placeable source. Auto-hide exists as a checkbox but can stay off. No Photoshop, no crop-filter stack.

- Plugin bootstrap + CI (Windows installer, plugin zip, source zip)
- Sample any source, masked draw
- Cutout editor
- Works on a vertical canvas and on a vanilla OBS scene

## v1 — optional auto-hide (public)

- Auto-hide checkbox, **off by default**
- Whole-instance hide when the element is gone
- Per-island hide when the cleaned mask has separate blobs
- **One shared presence cycle** for all visible auto-hide masks (grouped by sampled source)
- **Split into N sources** when cleanup finds multiple islands
- Hysteresis + fade; signatures ignore changing fill

Public release when cut-out is reliable and auto-hide is safe to leave off (or on) without wrecking a stream.

## After v1

- Better cleanup (eraser, multiple strokes, fewer merged islands)
- Tested macOS / Linux, not only CI packages

## Not doing

- Motion-based UI discovery
- Per-game mask libraries
- Full-frame autodetect with no highlight
- Auto-layout on the canvas
- Auto-resize of islands when HUD chrome grows/shrinks
- Shipping PNG files as the way to set up a game

## Issues

| Phase | Issues |
| --- | --- |
| Epic | [#1](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/1) |
| MVP | [#2](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/2) [#3](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/3) [#4](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/4) [#10](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/10) [#5](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/5) [#14](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/14) |
| v1 | [#7](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/7) [#8](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/8) [#16](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/16) |
| Research | [#15](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/15) |
| Closed | [#6](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/6) [#9](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/9) [#11](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/11) [#12](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/12) [#13](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/13) |

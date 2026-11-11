# Roadmap

Release plan is GitHub milestones. Tag the version when that milestone's issues are done. Dates are not promises. Shipped through 0.5.5. 0.6.0 is the next tag.

## Shipped

Through 0.5.5.

- Sample any source or scene, masked draw, place and scale the scene item. No auto-layout.
- Draw mask: paint, erase, line, shape, fill, Magic Select, Snap Edges, pause, undo/redo of the last 32 edits.
- Expand and Feather on the properties pane. Crop pad is 32px.
- Optional auto-hide, off by default, labeled experimental ([#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20), 0.3.0). One shared cycle, one readback per target, about 10 Hz. It compares edges to one Apply still. It is not a model.
- Magic Select with a finetuned MobileSAM decoder (0.4.0, [#45](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/45)). 0.6.0 replaces that call. The ONNX stack stays until [#75](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/75).
- Windows installer, portable zip, launch-time update check. OBS closes for the install and reopens after.
- Works on a vertical canvas and on a vanilla OBS scene.

The cut-out always works with auto-hide off. No telemetry. Windows only.

## 0.6.0 — Magic Select without a model

[Milestone](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/2). [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73). Includes the 0.5.6 correctness fixes ([#62](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/62)–[#71](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/71)). There is no 0.5.6 tag.

A missing mask draws nothing. A hidden target stays active. Scene lookup stays on its canvas. Undo scales with the frame. Freehand fill follows the curve. Auto-hide does not score a destroyed cutout. A silent update does not close OBS. Expand and Feather stay inside the 32px pad. Snap Edges reports success or failure. A failed Magic Select load does not leak sessions.

Keep the lasso and the name. On release, a classical pick (color and edges) runs inside the loop on a paused frame. No ONNX call. A miss does not replace the mask and says why. Snap Edges stays its own tool. Transparent and glowing HUD can still miss. Paint and erase stay the escape hatch.

Do not add Object Select, Magic Wand, or Quick Selection. Do not add click, box, or hover as a second mode. Do not add preview before apply. Do not train a decoder. Do not delete the ONNX stack in this release.

## 0.7.0 — auto-hide stills

[Milestone](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/3). [#74](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/74). [#67](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/67) is included in 0.6.0, not a separate tag.

The checkbox stays off by default. While it is on, mark the current crop show or hide. A matching hide still forces hide, even if a show still also matches and the old edge score would show. Otherwise a matching show still forces show. If none of the extra stills match, today's gate against the Apply still still applies. A labeled still still waits for the existing 3-frame streak. The Apply still is never replaced. A new Apply clears the extra stills.

The plugin does not know "menu". It only knows that saved crop. Two crops that are the same cannot be split. This is not a model and it will not be 100%. At most 8 stills per cutout. One readback, then compare. Auto-hide off means no analysis.

## Not scheduled

- [#75](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/75). Remove MobileSAM, the DLL, and the models after [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73) is the Magic Select people use. Do not delete `SAMHUDTraining` stills. Do not overwrite `data/models/mobilesam.decoder.onnx` in the meantime.
- macOS and Linux builds and installers
- Color tolerance for flat panels. Fill floods an outline today.
- Preview Expand and Feather in the editor. [#69](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/69) is included in 0.6.0.
- Magnetic tracing. Snap Edges is one-shot. [#70](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/70) is included in 0.6.0. Do not add a second tracer in 0.6.0.

## Not doing

- Training MobileSAM, a new decoder, or a learned Select Subject
- Object Select, Magic Wand, or Quick Selection as separate tools
- Training inside OBS, upload, a cloud model, or live inference
- Motion-based UI discovery
- A catalog of games, or shipped PNG masks
- Full-frame autodetect with no highlight ([#13](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/13), closed)
- Auto-placing items on the canvas
- Auto-resize of the scene item when HUD chrome grows or shrinks
- Auto-shrink inside the authored mask ([#41](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/41), closed)
- Split into N sources (closed; one Source Cutout per element)
- Telemetry, a paid tier, game memory, or injection
- Closed [#41](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/41), [#43](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/43), [#44](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/44), and [#46](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/46). Do not reopen them.

## Issues

| Milestone | Issues |
| --- | --- |
| [0.6.0](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/2) | [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73), and [#62](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/62)–[#71](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/71) (no 0.5.6 tag) |
| [0.7.0](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/3) | [#74](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/74) |
| Not scheduled | [#75](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/75) |
| Closed, do not reopen | [#41](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/41), [#43](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/43), [#44](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/44), [#46](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/46) |

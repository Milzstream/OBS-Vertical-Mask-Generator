# Roadmap

Release plan is GitHub milestones. Tag the version when that milestone's issues are done. Dates are not promises. Shipped through 0.5.5.

## Shipped

Through [0.5.5](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/releases/tag/0.5.5).

- Sample any source or scene, masked draw, place and scale the scene item. No auto-layout.
- Draw mask: paint, erase, line, shape, fill, Magic Select, Snap Edges, pause, undo/redo of the last 32 edits.
- Expand and Feather on the properties pane. Crop pad is 32px.
- Optional auto-hide, off by default, labeled experimental ([#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20), 0.3.0). One shared cycle, one readback per target, about 10 Hz. It compares edges to one Apply still. It is not a model.
- Magic Select with a finetuned MobileSAM decoder (0.4.0, [#45](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/45)). A missing model or DLL falls back to edge snap. That call is what [0.6.0](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/2) replaces.
- Windows installer, portable zip, launch-time update check. OBS closes for the install and reopens after.
- Works on a vertical canvas and on a vanilla OBS scene.

The cut-out always works with auto-hide off. No telemetry. Windows only.

## 0.5.6 — correctness

[Milestone](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/1). Bugs only. No new tools. MobileSAM still ships, so the ONNX leak stays here.

| Issue | What is wrong |
| --- | --- |
| [#62](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/62) | No mask, or Clear + Apply, draws the whole source. A missing mask should draw nothing. |
| [#63](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/63) | After a restart, a hidden target is not kept active, so the cutout goes black. |
| [#64](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/64) | A scene on an extra canvas can bind the main-canvas scene of the same name. |
| [#65](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/65) | Undo after a resolution change restores the old-size mask. |
| [#66](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/66) | Fill on a freehand Shape fills the chord, not the painted curve. |
| [#67](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/67) | Auto-hide can score a cutout after it has been destroyed. [#74](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/74) waits on this. |
| [#68](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/68) | A silent update can close OBS and then abort, leaving OBS shut with the old plugin. |
| [#69](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/69) | Expand and Feather go to 48. The saved mask only has 32px of padding, so the extra is clipped. |
| [#70](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/70) | Snap Edges does not show the success or failure string that already exists. |
| [#71](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/71) | Magic Select retries and leaks ONNX sessions when the model fails to load. |

## 0.6.0 — Magic Select without a model

[Milestone](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/2). [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73).

Keep the lasso and the name. On release, a classical pick (color and edges) runs inside the loop on a paused frame. No ONNX call. A miss does not replace the mask. Snap Edges stays the refine. Transparent and glowing HUD can still miss. Paint and erase stay the escape hatch.

Do not add Object Select, Magic Wand, or Quick Selection. Do not add click, box, or hover as a second mode. Do not add preview before apply. Do not train a decoder. Do not delete the ONNX stack in this release.

## 0.7.0 — auto-hide stills

[Milestone](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/3). [#74](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/74). After [#67](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/67).

The checkbox stays off by default. While it is on, mark the current crop show or hide. A matching hide still forces hide, even if a show still also matches and the old edge score would show. Otherwise a matching show still forces show. If none of the extra stills match, today's gate against the Apply still still applies.

The plugin does not know "menu". It only knows that saved crop. Two crops that are the same cannot be split. This is not a model and it will not be 100%. At most 8 stills per cutout. One readback, then compare. Auto-hide off means no analysis.

## Not scheduled

- [#75](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/75). Remove MobileSAM, the DLL, and the models after [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73) is the Magic Select people use. Do not delete `SAMHUDTraining` stills. Do not overwrite `data/models/mobilesam.decoder.onnx` in the meantime.
- [#41](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/41). Auto-shrink inside the authored mask. Blocked until [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73) is trustworthy. Does not resize the scene item. `docs/vision.md` still lists auto-resize as a non-goal. Do not build this on Sobel shrinkwrap or MobileSAM.
- macOS and Linux builds and installers
- Color tolerance for flat panels. Fill floods an outline today.
- Preview Expand and Feather in the editor, after [#69](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/69)
- Magnetic tracing. Snap Edges is one-shot. Do not add a second tracer until [#70](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/70) is done.

## Not doing

- Training MobileSAM, a new decoder, or a learned Select Subject
- Object Select, Magic Wand, or Quick Selection as separate tools
- Training inside OBS, upload, a cloud model, or live inference
- Motion-based UI discovery
- A catalog of games, or shipped PNG masks
- Full-frame autodetect with no highlight ([#13](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/13), closed)
- Auto-placing items on the canvas
- Auto-resize of the scene item when HUD chrome grows or shrinks
- Split into N sources (closed; one Source Cutout per element)
- Telemetry, a paid tier, game memory, or injection
- Closed [#43](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/43), [#44](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/44), and [#46](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/46). Do not reopen them.

## Issues

| Milestone | Issues |
| --- | --- |
| [0.5.6](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/1) | [#62](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/62)–[#71](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/71) |
| [0.6.0](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/2) | [#73](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/73) |
| [0.7.0](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/milestone/3) | [#74](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/74), after [#67](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/67) |
| Not scheduled | [#75](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/75), [#41](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/41) |
| Closed, do not reopen | [#43](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/43), [#44](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/44), [#46](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/46) |

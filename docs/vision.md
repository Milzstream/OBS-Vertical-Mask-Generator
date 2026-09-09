# Vision

## Problem

Horizontal games are 16:9. Vertical streams are 9:16. Rotating the game to fill a phone is a poor viewer experience. The usual layout is:

1. Crop the world into the middle of the vertical canvas.
2. Cut out HUD the crop just hid (minimap, bars, meters, abilities, …).
3. Place those cut-outs in the leftover space.
4. Scale them if needed.

Today that means source clone + crop filter + a handmade black-and-white PNG + Image Mask/Blend, then toggling visibility when the HUD disappears. See [manual-workflow.md](manual-workflow.md).

## Product

A new OBS source, **HUD Mask**.

It samples any other source, lets the user highlight an element on a live view, turns that highlight into a cropped alpha cut-out, and is placed like any scene item.

It is **not** Aitum-specific and **not** vertical-specific. Aitum Vertical is the intended canvas, not a dependency. There is no in-plugin list of games.

Optional **auto-hide** makes the cut-out transparent when that element’s outline is no longer in the sampled source. It is a checkbox, off by default, so a bad detector never makes the source worse than a static PNG. See [#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20).

## Setup

1. Add HUD Mask on any scene.
2. Choose the source (or scene + canvas) to sample.
3. Highlight the element (rough is enough): paint, line, fill, or Magic Select.
4. Snap Edges if you want the paint pulled onto chrome. Apply crops to the mask.
5. Drag and scale the item.

One instance is one cut-out. Use more HUD Masks for more elements (**Same Masks** lists the others on that target).

## Runtime

**Cut-out always works.** Manual scene-item visibility always still works.

When auto-hide is on:

- Presence is about the **element**, not the artwork inside it (cooldowns and meter numbers should not hide a bar).
- All visible auto-hide masks are scored in **one shared cycle** (minimap and abilities together, not a timer per source). Hidden items are skipped.

## Success

The cut-out is usable when:

1. Highlight → optional snap → crop produces a cut-out good enough to stream, with no Photoshop.
2. The item can live on a vertical canvas or a normal OBS scene.

Auto-hide is a later bar: optional, and when enabled it does not punch world through a hole on loading screens / hidden HUD, without flickering on changing icons, and without hitching a dual-canvas stream.

## Non-goals

- A catalog of games or shipped mask files
- Motion / “walk around while we detect UI”
- Auto-placing items on the canvas
- Auto-resize of islands when HUD chrome grows
- Building the rest of the vertical scene (world crop, cam, alerts, chat, branding)
- Game memory, injection, or Aitum-private APIs
- A paid product or telemetry
- Pixel-perfect outlines on the first stroke (erase and paint again)

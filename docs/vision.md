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

It samples any other source, lets the user highlight an element on a live view, turns that highlight into a cropped alpha cut-out, and is placed like any scene item. Optional auto-hide makes the cut-out transparent when that element is no longer in the sampled source.

It is **not** Aitum-specific and **not** vertical-specific. Aitum Vertical is the intended canvas, not a dependency.

## Setup

1. Add HUD Mask on any scene.
2. Choose the source to sample.
3. Highlight the element (rough is enough).
4. Cleanup hugs the chrome and crops to the mask.
5. The user drags and scales the item.

One instance can cover several disconnected blobs (islands) from a single highlight. There is no in-plugin list of games.

## Runtime

**Cut-out always works.** Auto-hide is a checkbox, off by default, so a bad detector never makes the source worse than a static PNG.

When auto-hide is on:

- Presence is about the **element**, not the artwork inside it (cooldowns and meter numbers should not hide a bar).
- **Islands** in one mask can hide independently (one of three ability circles gone → only that circle drops).
- If island detection is a poor fit, the fallback is one HUD Mask per blob.
- If a layout changes size enough that the mask is wrong, the user re-opens the editor (or uses separate instances). Automatic resize of islands is a later experiment, not the v1 bar.

Manual scene-item visibility always still works.

## Success

v1 is usable when:

1. Highlight → cleanup → crop produces a cut-out good enough to stream, with no Photoshop.
2. The item can live on a vertical canvas or a normal OBS scene.
3. Auto-hide is optional and, when enabled, does not punch world through a hole on loading screens / hidden HUD, without flickering on changing icons.
4. Dual-canvas streaming does not hitch.

## Non-goals

- A catalog of games or shipped mask files
- Motion / “walk around while we detect UI”
- Auto-placing items on the canvas
- Building the rest of the vertical scene (world crop, cam, alerts, chat, branding)
- Game memory, injection, or Aitum-private APIs
- A paid product or telemetry
- Pixel-perfect outlines on the first stroke (erase and paint again)

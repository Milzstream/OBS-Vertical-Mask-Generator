## Product

A **general OBS cut-out source** (working name **HUD Mask**).

Intended use: Aitum Stream Suite vertical canvas — crop the 16:9 world, then cut out HUD that the crop hid and drag those pieces into the leftover 9:16 space.

Technical scope: **any OBS source, any scene, any canvas.** Not Aitum-specific, not vertical-specific, not game-specific. Game capture, scene, browser, display capture, etc.

## Setup (one instance = one element)

1. Add Source → HUD Mask
2. Pick the source to sample
3. Live view + **highlighter** (rough is fine)
4. Plugin **cleans up** the highlight so it hugs the element, then **crops** to it
5. User drags / scales the scene item. The plugin does **not** auto-layout.

## Runtime

Cut-out always works. **Auto-hide is a checkbox, off by default.**

When on: islands (disconnected blobs in one mask) hide independently if that blob is gone. Whole source hides if every island is gone. Detect the **element**, not its contents (icons/numbers changing should not hide a bar).

All visible auto-hide masks share **one presence cycle**, grouped by sampled source. Hidden items are skipped.

If cleanup finds several blobs, **Split into N sources** creates one HUD Mask per island in a click.

If islands misbehave, **Split into N sources** or highlight each blob as its own HUD Mask. Layout changes: re-open the editor. No auto-resize.

## Not doing

- Game database / shipped Photoshop PNGs
- Motion / “walk around while we detect UI”
- Auto-placing cut-outs
- Auto-resize of islands when HUD chrome grows/shrinks
- Game memory / Aitum-private APIs
- Paid / telemetry

## Phasing

| Phase | Outcome |
| --- | --- |
| MVP | Highlight → clean → crop → placeable source on any canvas |
| v1 (public) | Optional auto-hide; split islands; Windows installer; repo can go public |
| After | Better cleanup; tested macOS/Linux |

## Docs

`docs/vision.md`, `architecture.md`, `feasibility.md`, `roadmap.md`, `manual-workflow.md`

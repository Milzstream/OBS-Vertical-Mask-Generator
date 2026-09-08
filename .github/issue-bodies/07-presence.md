## Phase

v1 — optional; public bar is “safe to leave off”

## Goal

**Auto-hide is a checkbox, off by default.**

With it off, HUD Mask is still useful: highlight → cut-out, no Photoshop. With it on, islands whose element is gone go transparent instead of showing world/loading through the hole.

## In scope

- Checkbox on the properties sheet; default **off** (zero analysis cost)
- Whole-instance hide when every island is gone
- Per-island hide when the cleaned mask has separate blobs (#8)
- Score the **slot/chrome**, not icons / numbers / minimap fill
- Small ROI, downscaled, ≤10 Hz, off the graphics thread
- Hysteresis + fade
- Scene-item visibility still wins

## Out of scope

- Auto-resize of islands (#12)
- Guaranteeing zero false hides
- Game memory

## Acceptance

- [ ] Auto-hide off: no extra CPU; cut-out still streams
- [ ] Loading / hide-UI: no world punching through when auto-hide is on
- [ ] Changing contents of a still-present bar does not flicker
- [ ] Dual-canvas stream does not hitch vs cut-out-only

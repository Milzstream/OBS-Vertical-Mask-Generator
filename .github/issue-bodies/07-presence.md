## Phase

v1 — optional; public bar is “safe to leave off”

## Goal

**Auto-hide is a checkbox, off by default.**

With it off, HUD Mask is still useful: highlight → cut-out, no Photoshop. With it on, islands whose element is gone go transparent instead of showing world/loading through the hole.

Checks run in **one plugin-wide cycle**, not a timer per mask. Minimap + abilities (and any other visible HUD Mask sampling the same game) are scored from the same downsample. Hidden scene items and inactive scenes are skipped. Adding more masks on the same target must not add more GPU readbacks.

## In scope

- Checkbox on the properties sheet; default **off** (zero analysis cost)
- Module-level presence pass, ≤10 Hz
- Group visible, auto-hide-on instances by sampled target
- One downsample/readback per unique target per cycle
- Whole-instance hide when every island is gone
- Per-island hide when the cleaned mask has separate blobs (#8)
- Score the **slot/chrome**, not icons / numbers / minimap fill
- Hysteresis + fade
- Scene-item visibility still wins

## Out of scope

- Auto-resize of islands (#12)
- A thread or GPU readback per HUD Mask instance
- Checking sources that are hidden or not on an active canvas
- Guaranteeing zero false hides
- Game memory

## Acceptance

- [ ] Auto-hide off: no extra CPU; cut-out still streams
- [ ] Two visible masks on the same game capture: **one** presence readback per cycle
- [ ] A hidden HUD Mask is not in the cycle
- [ ] Loading / hide-UI: no world punching through when auto-hide is on
- [ ] Changing contents of a still-present bar does not flicker
- [ ] Dual-canvas stream does not hitch vs cut-out-only

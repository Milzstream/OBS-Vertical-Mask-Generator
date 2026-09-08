## Phase

v1 (setup UX; no runtime cost)

## Goal

If cleanup finds multiple islands (e.g. three ability circles), the editor offers **Split into N sources**.

One click creates N HUD Mask sources (same sampled target, one island each), places them so the composite still lines up on the canvas, and removes the original combined source. The user does not highlight the bar three times.

Each resulting source can hide, move, and scale independently when kits differ.

## In scope

- Detect island_count > 1 after cleanup
- Button / prompt: Split into N sources
- Create N sources in the current scene with per-island crop + mask
- Preserve world placement so the look does not jump
- Remove or replace the combined source
- Undo should be possible if OBS undo covers source create/delete; if not, say so in the UI

## Out of scope

- Auto-split with no confirmation
- Auto-resize of a grown circle (not doing)
- Merging sources back into one (nice later, not required)

## Acceptance

- [ ] Three-circle ability highlight → Split → three scene items that still look like the original bar
- [ ] Each can be transformed and auto-hidden on its own
- [ ] Faster than opening the editor three times
- [ ] No extra stream-time cost vs having created them by hand

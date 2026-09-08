## Phase

MVP

## Goal

Stock properties sheet is small. The highlighter lives in a custom window (#10). This issue is the glue.

## In scope

- Sampled-source dropdown (any source type)
- Button **Edit cutout…** that opens the highlighter window
- Persist: target source, mask, crop, feather
- Re-open editor later to redo a highlight
- Optional: invert, feather slider

## Out of scope

- Crop inset spinboxes as the primary setup (that is the old Photoshop workflow)
- Presence controls until #7 (can be a hidden/disabled group)

## Acceptance

- [ ] Settings survive OBS restart / scene collection reload
- [ ] Changing the sampled source updates the live editor
- [ ] Invalid target does not crash

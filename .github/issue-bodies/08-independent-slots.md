## Phase

v1

## Goal

A single highlight over several disconnected blobs (e.g. three ability circles) becomes **islands**. Auto-hide can drop one island and leave the others.

Example: an ability bar drawn as three circles. Some kits only show the right two — only the left circle should disappear. The user should not have to add three HUD Mask sources for that, though that remains the fallback.

## In scope

- Connected-component labeling after cleanup
- Per-island presence signature and score
- Output mask = union of present islands
- Scene item size stays the full crop (no jump)
- If blobs merge, document “use one source per blob”

## Out of scope

- Auto-growing a circle to a larger size (not doing; split or re-edit instead)
- Splitting a single connected bar into fake slots

## Acceptance

- [ ] Three-circle mask: hiding the left blob leaves the other two
- [ ] All three gone → whole source transparent
- [ ] One solid panel → one island (no spurious splits)
- [ ] Fallback of three separate HUD Mask sources still works

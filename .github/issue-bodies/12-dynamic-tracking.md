## Phase

After v1 — experimental

## Goal

If an island **changes size** (circle grew to match its neighbors, meter taller in combat), optionally update that island’s mask instead of only hide/show.

v1: re-open the editor, or use separate sources. This issue is only if that is not enough.

## Constraints

- Must not grab world pixels
- Must not jump the scene item
- Must stay in the presence CPU budget (no full-frame, no 60 fps CV)
- Allowed to fail and be closed

## Acceptance

- [ ] Spike notes with a resizing panel and a same-bar-larger-circles case
- [ ] If brittle, keep v1 hide/show + manual re-edit

## Phase

v2

## Problem

NTE ability chrome is not the same for every character. A 3-circle PNG on a 4-slot kit (or the reverse) leaves empty space or clips icons. Static PNGs force a compromise on look vs completeness.

## Goal

For **circle-like ability HUD only** (start with NTE):

- Detect circles in the crop
- Generate a feathered mask from the detected set
- If count is 0 → treat as “not present” (hide)
- If count changes → rebuild the mask (slow timer, not every frame)

## In scope

- NTE abilities as the experiment
- Hide when no circles
- Optional stored variants (3-slot vs 4-slot) with best-template pick as a simpler fallback

## Out of scope

- Every HUD type
- Morphing health-bar fill every frame
- Character identification by name

## Acceptance

- [ ] Character A (3 slots) and character B (4 slots) both look acceptable without swapping PNGs by hand
- [ ] Menu / car (0 slots) hides
- [ ] If circle detection is too brittle, fall back to variant templates and document that

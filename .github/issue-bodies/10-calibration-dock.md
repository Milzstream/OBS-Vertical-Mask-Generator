## Phase

v1.5

## Problem

Typing crop insets from a Photoshop guide is the slow part of setup. Pixel-perfect tracing is the other slow part.

## Goal

A dock (or properties preview) where the user:

1. Snapshots the sampled source
2. **Roughly circles or boxes** each HUD piece (does not need to be exact)
3. Plugin takes that smaller ROI, detects edges / floods the chrome, feathers a mask
4. Shows a live preview of the masked result
5. Saves crop + mask + (later) presence reference onto the HUD Mask source

This is the primary “as much automation as possible” setup UX that is still technically honest.

## In scope

- Snapshot of the target source
- Draw rectangle and/or loose lasso
- Edge / threshold / flood-fill snap inside the mark
- Preview overlay
- Write results into the selected HUD Mask source

## Out of scope

- Full-frame “find everything” with no user mark (see the movement-assist issue)
- Replacing the branding overlay
- Photoshop export

## Acceptance

- [ ] On an NTE combat snapshot, a loose box around the minimap produces a circular-ish mask close to `Radar/NTE.png`
- [ ] A loose box around abilities produces separate circles or one combined mask the user can accept
- [ ] User can reject and redraw
- [ ] Resulting source still works on the vertical canvas

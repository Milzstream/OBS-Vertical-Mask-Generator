## Phase

MVP

## Problem

Crop math today lives in a Photoshop reference image and OBS crop filter spinboxes. The new source needs the same controls in one properties dialog.

## Goal

HUD Mask properties a streamer can set without a dock:

- Source picker (existing sources)
- Crop left / top / right / bottom
- Mask PNG file path + small preview
- Feather (0 = use PNG as-is)
- Optional: “include target filters”
- Optional: opacity (for testing)

## In scope

- Standard OBS properties (`obs_properties`)
- Persist settings in the scene collection
- Preview of the mask file in the dialog if OBS allows it easily

## Out of scope

- Drawing a rectangle on a snapshot (that is the calibration dock, v1.5)
- Presence controls (v1 — can be a disabled/hidden group until then)

## Acceptance

- [ ] Settings survive OBS restart
- [ ] Changing crop/mask updates the vertical canvas live
- [ ] Crop language matches current notes (insets from each edge, not x/y/w/h only — showing both is fine)
- [ ] Invalid file path does not crash

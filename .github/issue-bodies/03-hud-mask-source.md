## Phase

MVP

## Problem

Each HUD piece is currently a Source Clone + Crop filter + Image Mask/Blend. That is three objects per element and no place to later attach presence detection.

## Goal

One **HUD Mask** source that:

1. Picks an existing OBS source (the raw game capture / scene)
2. Renders it to a texrender (Source Clone pattern)
3. Crops with OBS-style left/top/right/bottom insets
4. Applies a white-on-black PNG mask
5. Outputs RGB from the game and alpha from the mask

## In scope

- `obs_source_video_render` / `gs_texrender` sampling
- Crop insets matching the Crop filter (see `docs/current-workflow.md` — NTE radar: left 45, top 10, right 2180, bottom 1065 on 2560×1440)
- PNG mask sampled as alpha (white = keep, black = discard, feathered edges preserved)
- Property: whether to include the target source’s filters
- No audio

## Out of scope

- Presence detection
- Auto crop
- Generated masks
- Aitum-specific hooks

## Technical notes

- Follow Exeldro Source Clone for canvas-safe sampling; Stream Suite extra canvases have bitten clone plugins before.
- Do not GPU→CPU read back the full frame on the graphics thread.
- SDR first. HDR color space is a later footgun.

## Acceptance

- [ ] Pointing at the game source + NTE radar crop + `Radar/NTE.png` matches the current clone+crop+mask look on the vertical canvas
- [ ] Same for NTE abilities + `Abilities/NTE.png`
- [ ] Mask feather looks like Image Mask/Blend, not a hard cut
- [ ] Missing target source → draw nothing, no crash
- [ ] Two HUD Mask instances can be active at once (radar + abilities)

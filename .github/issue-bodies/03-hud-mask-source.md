## Phase

MVP

## Goal

HUD Mask samples **any** other OBS source, draws only the cut-out (mask + crop), outputs alpha. The mask/crop come from the highlighter editor (#10), not from a Photoshop file.

## In scope

- Sample scene / game capture / display capture / browser / etc. (`obs_source_video_render` + `gs_texrender`)
- Crop to the bounding box of the current mask
- Apply mask as alpha (white/keep, feathered)
- Missing target → draw nothing, no crash
- Multiple instances at once
- Works with the target on a different canvas than this source (Stream Suite extra canvas is the hard case; vanilla single-canvas must work too)

## Out of scope

- Highlighter UI (that's #10)
- Presence / auto-hide (#7)
- PNG library of games

## Acceptance

- [ ] Three instances can reconstruct a WoW-style vertical layout (meter, minimap, action bar) **if** masks are provided by the editor
- [ ] Same for NTE radar + abilities as a second test case
- [ ] Works when HUD Mask sits on the main canvas too (not only vertical)
- [ ] SDR first; no crash on missing source

## Phase

MVP

## Problem

This plugin only matters if it works on the **Aitum Stream Suite vertical canvas**, not just the main OBS canvas. Extra-canvas source sampling has historically been buggy in clone plugins.

## Goal

Confirm HUD Mask is a normal scene item on Stream Suite’s vertical canvas: add, transform, blend, hide, hotkey, copy.

## In scope

- Manual test matrix on OBS 32 + Stream Suite (the environment already in use)
- Sampling a game source that lives on the **main** canvas while HUD Mask lives on the **vertical** canvas
- Notes in `docs/` of any Stream Suite quirks

## Out of scope

- Linking against Stream Suite
- Scene-link / dual-scene automation
- Supporting the standalone Aitum Vertical plugin as well as Stream Suite in v1 (nice if it works; not a gate)

## Acceptance

- [ ] HUD Mask added to AITUM VERTICAL appears and renders
- [ ] Transform (move/scale) works in that canvas
- [ ] Target source can be the main-canvas game capture
- [ ] No crash when switching scene collections or toggling the vertical dock
- [ ] Dual-canvas stream/preview does not hitch vs the current clone stack (subjective, but no extra full-frame copy)

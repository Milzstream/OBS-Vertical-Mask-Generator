## Phase

v1

## Problem

Each game is a bundle of slots (NTE = radar + abilities + level bar) with crops and masks. Rebuilding that in OBS properties every time is the same pain as today.

## Goal

A JSON game profile that can stamp settings onto HUD Mask sources. Schema already lives in `profiles/schema.json`.

## In scope

- Load a profile from disk
- Create or update HUD Mask sources for each slot (or dump settings the user applies)
- First packed example: NTE (radar, abilities, level bar) using **paths** to the existing OneDrive PNGs, not the binaries in git
- `schemaVersion: 1`

## Out of scope

- Auto-download community profiles
- Character variants (v2)
- Storing PNGs in the repo

## Acceptance

- [ ] A NTE profile file describes the three slots with crop insets
- [ ] Applying it is faster than setting three sources by hand
- [ ] Invalid JSON fails cleanly

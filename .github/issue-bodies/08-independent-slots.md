## Phase

v1

## Problem

NTE (and others) do not hide the whole HUD at once. In a vehicle the minimap stays and abilities disappear. A single “HUD visible?” flag would hide too much.

## Goal

Presence is **per HUD Mask instance**. Radar and abilities are two sources with two references.

## In scope

- No global “game is in menu” detector
- Profiles may list multiple slots, but runtime hide is per source
- Document this as the recommended scene setup

## Out of scope

- One source that composites every slot internally (maybe later; not v1)

## Acceptance

- [ ] NTE car: ability HUD Mask transparent, radar HUD Mask still showing
- [ ] NTE menu: both hide if both HUDs are gone
- [ ] Disabling presence on one instance does not affect the other

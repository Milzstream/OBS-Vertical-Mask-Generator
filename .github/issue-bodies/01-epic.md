## Product

An OBS Studio plugin that extracts HUD / UI pieces from a horizontal game source and places them on an Aitum Stream Suite vertical canvas.

Today that is: source clone → crop filter → handmade Photoshop PNG mask → transform on the vertical canvas. It looks right only when the HUD is present and matches the PNG. Menus, loading, cutscenes, vehicles, and character-specific ability bars punch holes of world/background through the overlay.

## Form

A new OBS **source** named **HUD Mask** (not a filter, not a Stream Suite fork):

- Sample an existing source (raw game capture / scene)
- Crop to one HUD element
- Apply a mask (imported PNG first; generated later)
- Output alpha so it can be placed, scaled, and dragged on the vertical canvas
- Optionally fade to transparent when that HUD is not actually on screen

One instance per HUD element so abilities can hide while the minimap stays (NTE car example).

## Phasing

| Phase | Outcome |
| --- | --- |
| MVP | One source replaces clone + crop + PNG. Same look as today. |
| v1 | Presence detection hides the item when the HUD is gone. |
| v1.5 | Calibration dock: user roughly marks HUD, plugin snaps edges. |
| v2 | Movement-assisted isolation, generated masks, circle-count tracking. |

## Constraints (locked)

- **Free to use** — GPL-2.0-or-later, no paid tier, no license key, no telemetry. GPL is required to link libobs.
- **No game-memory reading / injection** — anti-cheat (Destiny and others).
- **No Aitum-private API** — it is a normal OBS source on the vertical canvas.
- **Windows + OBS 32 + Stream Suite first.**
- Existing PNGs in `Vertical UI Masks` must keep working.

## Docs in this repo

- `docs/vision.md`
- `docs/current-workflow.md`
- `docs/architecture.md`
- `docs/feasibility.md`
- `docs/roadmap.md`

Child issues track the work. This issue is the product brief.

## Phase

v1 — this is the actual differentiator vs a static PNG

## Problem

When the HUD is gone, the mask still punches holes:

- NTE menu / exploration: ability circles show the city
- NTE in a car: ability circles show the road; minimap should stay
- Loading screens and cutscenes: same bleed

Micromanaging visibility per source is the current workaround.

## Goal

Each HUD Mask instance can **fade to fully transparent** when its HUD chrome is not in the crop, and fade back in when it returns — without reading game memory.

## In scope

- Presence off by default (must calibrate)
- **Probe** mode: N sample points of HUD chrome
- **Template** mode: small grayscale snippet of chrome (not the moving map interior)
- GPU→CPU readback of the **crop only**, downscaled, ≤10 Hz, off the graphics thread
- Hysteresis (hold ms) + 150–300 ms opacity fade
- “Capture reference” button while the HUD is visible
- Manual override = stock source visibility still wins

## Out of scope

- Game memory / anti-cheat
- Morphing the mask every frame
- Full-frame analysis
- Guaranteeing zero false hides

## Test matrix (NTE, from the screenshots we have)

- [ ] Combat, HUD present → abilities and radar visible
- [ ] Menu / HUD hidden → ability holes **do not** show the background
- [ ] In a vehicle → abilities hide, radar can stay (second instance)
- [ ] Walking around with HUD present → does not flicker
- [ ] A too-low threshold that would flicker is tunable

## Notes

Calibrate on **chrome** (minimap ring, ability slot frames), not on the fill. The fill is the world or changing icons.

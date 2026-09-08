## Phase

v1 — public/stable bar

## Goal

When the **highlighted element** is gone from the sampled source, this instance fades out. When it is back, it fades in. No game memory. No game list.

## In scope

- Off until a cutout exists
- Signature of the **element** (chrome / panel), not spell icons / meter numbers / minimap terrain
- Read back the **crop only**, downscaled, ≤10 Hz
- Hysteresis + fade
- Manual visibility still wins

## Test cases (examples, not a catalog)

- WoW: `Alt+Z` or loading screen → action bar, meter, minimap cut-outs hide; they return in the world
- WoW: pressing spells must **not** hide the action bar
- NTE: menu / vehicle hides abilities; radar can stay (#8)
- A game we have never authored a PNG for: highlight something, hide UI in-game, cut-out hides

## Out of scope

- Morphing the mask every frame
- Resizing Details-style meters (after v1, #12)
- Guaranteeing zero false hides

## Acceptance

- [ ] Loading / hidden-HUD does not punch world through the cut-out
- [ ] Changing contents of a still-present bar does not flicker
- [ ] Tunable threshold

# Feasibility

## Does this already exist?

No. See [prior-art.md](prior-art.md). Sampling and PNG masks are solved. The missing product is highlight → cleanup → placeable source, with optional presence.

## Straightforward

| Piece | Notes |
| --- | --- |
| Sample any OBS source | Same idea as Source Clone |
| Live preview + highlighter | Custom Qt dialog |
| Crop + masked draw | GPU, cheap |
| Auto-hide checkbox | Off = zero analysis cost |
| Persist mask on the source | Scene collection settings |

## Highlight cleanup

Realistic for high-contrast chrome (circular minimap, ability rings, a dark meter panel). Weaker on translucent HUD, icons that look like the world, and soft glows. Escape hatch: erase and paint again. First-stroke outlines will not match a hand-traced PNG.

Cleanup should **keep gaps** between blobs so islands stay separate.

## Auto-hide (optional)

**Whole instance.** If every island is gone (loading screen, hide-UI key), the source is fully transparent. This is the easy win.

**Per island.** After cleanup, each connected blob is scored on its own. One of three ability circles missing → only that circle’s alpha goes to zero; the other two stay. Signatures must be the **slot**, not the icon inside it, or cooldowns will flicker.

Per-island hide is feasible when:

- Blobs do not touch in the cleaned mask
- Each blob has some stable chrome
- Scoring is a small ROI at a few Hz, not 4K 60 fps

It is **not** guaranteed for every HUD. Fallback: **Split into N sources** (one click after cleanup), not highlighting the bar three times by hand.

Auto-hide will never be 100%. That is why it is a checkbox. With it off, the plugin still replaces Photoshop + crop filters.

Presence is **one shared cycle** for all visible, auto-hide-on masks. Minimap + abilities both sampling the game are scored from the same downsample. Cost must not grow linearly with mask count.

## Split into N sources (setup)

If cleanup finds 3 blobs, offer to explode that HUD Mask into 3 sources. Cheap, no runtime cost, and each piece can hide or be transformed on its own. Better UX than auto-resize for “this kit only has two circles.”

## Auto-resize (later, optional)

If a circle grows to match its neighbors, or a meter gets taller in combat, updating that island’s mask is possible in principle (search a padded box for a similar blob). It is easy to get wrong (layout jump, grabbing world pixels) and is **not** the v1 bar. Re-opening the editor, or splitting into separate sources, is the supported way to handle a layout that changed size.

## Will not do

| Idea | Status |
| --- | --- |
| Walk around to detect UI from motion | Not doing |
| In-plugin game database | Not doing |
| Auto-place items on the canvas | Not doing |
| Unsupervised full-frame HUD find | Not v1 |
| Game memory / injection | Never |
| Aitum-private API | Never |

## Performance

- Auto-hide off: extra GPU blit of a small crop. Should be cheaper than the clone+mask stack it replaces.
- Auto-hide on: one plugin-wide cycle, ≤10 Hz. GPU scales with unique **visible** sampled targets, not with how many HUD Masks exist. Hidden / other-scene items are not in the cycle.
- Must not add a noticeable hitch to dual-canvas streaming or to the game.

## Platforms

Build Windows, macOS, and Linux via the plugin template. Test Windows + OBS 32 + Stream Suite first. Broken macOS/Linux artifacts at v1 are bugs, not release blockers.

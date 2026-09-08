# Current workflow

This is the baseline the plugin must be able to match, then beat.

## Layout in OBS

Two canvases side by side:

- **MAIN CANVAS** — 16:9 game (example: 2560×1440).
- **AITUM VERTICAL** — 9:16 composition. Gameplay is a cropped/scaled view of the same game. HUD extracts sit on top of (or in holes of) a branding bar.

The branding bar at the bottom of the vertical canvas (MilzOGram / Milzstream, circular slots) is **not** produced by these masks. The masks fill those slots with game HUD.

## Per-element recipe

1. Source clone the raw game scene (not the already-composed main scene, so extra overlays are not baked in).
2. Crop filter: left/top/right/bottom such that the clone is just larger than the HUD piece.
3. Image Mask/Blend: PNG from `C:\Users\Milz\OneDrive\Streams\Vertical UI Masks`.
4. Transform the clone on the vertical canvas (position, scale). Visibility is toggled by hand when the HUD disappears for a long stretch.

## Mask files on disk

```
Vertical UI Masks/
  Abilities/          NTE, Stellar-Blade
  Health Bar/         AlienIsolation, Marathon, Stellar-Blade
  Level Bar/          NTE
  Radar/              Destiny, NTE
  Weapons and Ammo/   Destiny, Marathon
  Creating Masks.png  crop-math reference
```

Each type folder has a `.png` plus the `.psd` it was made from.

### Mask conventions

- White = keep (visible HUD).
- Black = discard (transparent).
- Soft/feathered edges, not hard 1-pixel cuts. Circles (radar, NTE abilities) are especially feathered.
- Authored at **crop resolution**, not full frame.
- Shape follows the chrome, not a bounding box. Destiny weapons is a diamond + bars + magazine grid. Stellar Blade health is a cluster of bars plus a small icon. NTE level bar is a stadium with end-caps.

## Crop math (from Creating Masks.png)

For a 2560×1440 source, NTE radar was boxed as:

| Edge | Pixels from that edge |
| --- | --- |
| Left | 45 |
| Top | 10 |
| Right | 2180 |
| Bottom | 1065 |

Resulting crop: **335 × 366**. The PNG is a feathered circle inside that rectangle.

OBS crop filters use "crop from each edge", which is why the reference image is labeled that way. The plugin's crop UI should speak the same language so existing notes transfer.

## Where HUD lives on the 16:9 frame (examples)

| Game | Element | Typical origin on main canvas |
| --- | --- | --- |
| NTE | Radar / minimap | Top-left |
| NTE | Abilities | Bottom-right |
| NTE | Level bar | Bottom-center (horizontal) |
| Destiny | Radar | Top-left (circle) |
| Destiny | Weapons / ammo | Bottom (asymmetric) |

On the vertical canvas these are **not** kept in those corners. They are pulled into a composed overlay (often the bottom bar), which is why the item must be freely transformable after extraction.

## Failure cases we have screenshots of (NTE)

1. **Menu / exploration UI hidden** — ability mask still applied; circular holes show the city. Radar area also wrong if the radar is gone. The plugin must hide, not just mask.
2. **Different character, different ability chrome** — a mask built for one kit does not fit another. Empty space or clipped icons. Static PNGs cannot be universal across characters.
3. **Gameplay, HUD present, mask matches** — this is the look we want to keep.
4. **In a car** — abilities gone, minimap still there. Ability holes show the road. Radar should stay; abilities should not.

## What "better than this" means

Anything we ship has to be at least as controllable as this stack (I can still pick the crop, still use a PNG I made, still place the item). The first real upgrade is **not showing a hole when the HUD is missing**. The second is **less Photoshop per game**.

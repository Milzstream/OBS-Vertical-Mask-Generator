# Current workflow (the baseline we are replacing)

This is how vertical UI is done **today**. The plugin should make this stack unnecessary. It is not the setup UI we are building.

## Layout

Two canvases:

- **MAIN CANVAS** — 16:9 game.
- **AITUM VERTICAL** — 9:16. Center crop of the world, plus UI cut-outs moved into the leftover space.

The branding bar (Milzstream.tv / MilzOGram) is a separate graphic. Cut-outs sit on or around it.

## Per-element recipe (to delete)

1. Source clone the raw game (or capture).
2. Crop filter, just larger than the UI.
3. Image Mask/Blend with a Photoshop PNG from `Vertical UI Masks`.
4. Transform on the vertical canvas.
5. Toggle visibility by hand when the UI disappears.

## Examples we have

### NTE (previous screenshots)

- Radar / minimap from the **top-left** of 16:9, placed on the vertical overlay.
- Abilities from the **bottom-right**, placed in circular holes on the vertical bar.
- Failure: menu and vehicle still punch world through those holes because the PNG does not know the HUD is gone.
- Failure: character kits change ability chrome; one PNG does not fit all.

### WoW Classic (screenshot)

- World is center-cropped on vertical.
- **Details damage meter** (dark box, bottom of the 16:9 frame) is cut out and placed **top-left** on vertical.
- **Minimap** is cut out and placed **top-right** on vertical.
- **Action bars** are cut out, scaled to vertical width, and placed along the **bottom**. Close enough for mobile; not pixel-perfect.
- Player frames / extra HUD pieces follow the same idea.

Same job as NTE. Different game. No reason for the plugin to know the word “WoW.”

## Mask files on disk (should become unnecessary)

```
Vertical UI Masks/
  Abilities/          NTE, Stellar-Blade
  Health Bar/         AlienIsolation, Marathon, Stellar-Blade
  Level Bar/          NTE
  Radar/              Destiny, NTE
  Weapons and Ammo/   Destiny, Marathon
  Creating Masks.png  crop-math reference
```

Convention: white = keep, black = discard, feathered, authored at **crop size**. Useful as a visual target for how cleanup should look. Not an import requirement for v1.

### Crop math example (NTE radar, 2560×1440)

OBS crop insets: left 45, top 10, right 2180, bottom 1065 → 335×366, then a circular PNG.

The plugin should compute this box from the cleaned highlight instead of asking for these numbers.

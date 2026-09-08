# Prior art

Researched 2026-09-08. The packaged product does not already exist. Vertical HUD tutorials still teach clone + handmade PNG + Image Mask/Blend.

## Common vertical HUD stack

- [Source Clone](https://obsproject.com/forum/resources/source-clone.1632/)
- Black/white PNG (Photoshop, Photopea, Illustrator, SVGator)
- Image Mask/Blend or Advanced Masks image mask
- Place on Aitum Vertical

Some tutorials ship **per-game mask PNG packs**. None generate the mask from a highlighter. None hide the clone when the HUD is gone.

## Nearby plugins

| Tool | What it does | Gap |
| --- | --- | --- |
| OBS Image Mask/Blend | Apply a PNG | Last step of the manual stack |
| [Advanced Masks](https://github.com/FiniteSingularity/obs-advanced-masks) | Circles/rects/hearts, image or source as mask | Manual shape. No highlighter, no presence |
| [Visual Crop](https://obsproject.com/forum/resources/visual-crop.2331/) | Drag a rectangle crop | Rectangle only |
| Polymask | 8-point polygon sliders | Manual |
| [Advanced Scene Switcher](https://github.com/WarmUpTill/SceneSwitcher/wiki/Video-condition) video condition | Pattern-match a region; show/hide items | Closest auto-hide. Still needs a handmade cut-out. Macro box, not a source |
| Pixel Match Switcher | Pixel templates | Requires a forked OBS |
| Source Clone | Sample another source | Useful internally; not a HUD tool |
| AI webcam matting / object detect | People vs background | Wrong domain |

Aitum Vertical / Stream Suite add a canvas. They do not extract HUD.

## Why the gap exists

Not a missing OBS API. Sample + mask is proven.

1. Dual-canvas vertical is still new; the community publishes PNG packs.
2. Highlight → edge-snap is setup-time CV, not a 60 fps GPU problem, and nobody packaged an editor for it.
3. Auto-hide is possible (pattern match) but easy to get wrong on changing HUD fill.
4. Combining sample + shaped cut-out + placeable source + optional presence into one add-source flow is what is missing.

This project should reuse those ideas (texrender sampling, small-template match, feathered alpha), not reimplement Advanced Masks or Source Clone as the product.

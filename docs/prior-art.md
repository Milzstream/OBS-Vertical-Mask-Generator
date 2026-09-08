# Prior art — does this already exist?

Researched 2026-09-08. Short answer: **the packaged product does not exist.** The pieces do. Vertical HUD tutorials in 2025–2026 still teach the same manual stack you already use.

## What people actually do for vertical HUD

This is the standard advice, not a niche:

- [Source Clone](https://obsproject.com/forum/resources/source-clone.1632/) the game
- Make a black/white PNG in Photoshop / Photopea / Illustrator / SVGator
- Image Mask/Blend (or Advanced Masks image mask)
- Alt-crop and place on Aitum Vertical

Examples of that workflow being taught *as the solution*:

- “The Ultimate OBS Setup for Vertical Clips” — screenshot, pen-tool trace, clone, mask
- Apex Legends vertical tutorials that **ship downloadable game-specific mask PNGs**
- Aitum + Source Clone HUD tutorials (Devil May Cry, FGC, etc.)
- “How To Make A Professional Vertical Stream” — Illustrator cut-outs + Advanced Scene Switcher for *other* automation

Nobody in that pipeline generates the mask from a highlighter, and nobody hides the clone when the HUD is gone. They micromanage or live with holes.

## Nearby plugins (what they are / are not)

| Thing | What it does | vs this project |
| --- | --- | --- |
| **OBS Image Mask/Blend** | Apply a PNG you made | The last step of today’s stack |
| **[Advanced Masks](https://github.com/FiniteSingularity/obs-advanced-masks)** | Circles/rects/hearts, image mask, source-as-mask, scene-view transform | Better *manual* masking. You still define the shape. No highlighter, no HUD presence |
| **[Visual Crop](https://obsproject.com/forum/resources/visual-crop.2331/)** (2026) | Dock: drag a **rectangle** crop on a live preview | Rectangle only. No shaped HUD, no hide |
| **Polymask** | 8-point polygon sliders | Manual, ugly UX |
| **[Advanced Scene Switcher](https://github.com/WarmUpTill/SceneSwitcher/wiki/Video-condition)** video condition | Pattern-match a region; show/hide items | Closest **auto-hide**. You still author the cut-out and the template. General automation, not a cut-out source. Can be CPU-heavy |
| **Pixel Match Switcher** | Pixel templates → show/hide | Needs a **forked OBS**. Not a real option |
| **Source Clone** | Sample another source with its own filters | We will do this internally. It is not a HUD tool |
| **AI matting / webcam background** | Person vs background | Wrong domain |
| **obs-detect** | Detect people/objects, optional mask | Not game HUD |
| **Minecraft OBS Overlay mod** | Hide HUD *in the game* from capture | Game-specific injection. Opposite of a general OBS cut-out |

Aitum Vertical / Stream Suite only add a canvas. They do not extract HUD.

## Why this plugin is not already on the forums

Not because OBS cannot sample a source or apply a mask. That path is boring and proven.

The gap is **product-shaped**:

1. Vertical dual-canvas only became common recently (Aitum 2023, Stream Suite 2026). The community is still publishing Photoshop mask packs.
2. Highlight → edge-snap is a **setup-time** CV problem (GrabCut / watershed). It is not a 60 fps GPU problem. Nobody packaged an editor for it.
3. Auto-hide is possible (AdvSS already pattern-matches) but easy to get wrong on **changing HUD contents** (cooldowns, meter numbers). That is quality work, not a physics wall.
4. Combining (sample + shaped cut-out + placeable source + presence) into **one add-source flow** is the thing that does not exist.

## What that means for us

- We are not blocked by a missing OBS API.
- We should **not** rebuild Advanced Masks or Source Clone as the product.
- We **can** steal ideas: Source Clone’s texrender, AdvSS’s “match a small template with threshold + area,” Advanced Masks’ feathered alpha.
- The bet is the **cutout editor + element presence**, which is exactly what the tutorials still leave as homework.

If highlight-cleanup is too weak on real HUDs, the fallback is still better than today: paint a rough mask yourself in the editor (even if cleanup only feathers it) and get auto-hide. That is already a plugin people do not have.

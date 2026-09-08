# Architecture

## Form factor

OBS plugin (C++/Qt), new **input source**. GPL-2.0-or-later.

| Choice | Why |
| --- | --- |
| Source, not filter | Needs its own scene item to drag and scale. |
| Sample any source | Game capture, scene, display, browser — same OBS API. |
| No game database | Settings persist on the source in the scene collection. |
| Custom Qt editor | Stock properties cannot host a live video + highlighter. |
| Windows first, multi-platform CI | obs-plugintemplate already builds Win installer + zip, macOS pkg, Linux deb, source archive. |

## Add-source flow

```
Add Source → HUD Mask → name it
        ↓
┌─────────────────────────────────────────┐
│  Sample: [ Game Capture          ▼ ]    │
│                                         │
│  ┌─────────────────────────────────┐    │
│  │  live view of sampled source    │    │
│  │  highlighter over the element   │    │
│  └─────────────────────────────────┘    │
│  highlight · erase · reset · cleanup    │
│                           [ OK ]        │
└─────────────────────────────────────────┘
        ↓
source size = bounding box of the cleaned mask
user transforms the scene item
```

The normal properties sheet is small: sampled source, **Edit cutout…**, feather, invert, **Auto-hide** checkbox.

## Mask islands

After cleanup, the mask is labeled into **connected components** (islands). A highlight over three separate ability circles should yield three islands. A single solid panel is one island.

Each island stores:

- Pixel mask
- Bounding box (in the crop)
- Presence signature (chrome, not fill)

The scene item’s size stays the **full crop**. Hidden islands become transparent holes; the item does not jump on the canvas.

If cleanup merges blobs that should stay separate, the user either tightens the highlight or uses one HUD Mask per blob.

## Runtime

```
Every frame (GPU)
  sample target into a texrender
  draw crop through the current mask
  (optional) zero alpha on islands scored absent

If auto-hide is on, a few times per second
  (small ROI per island, downscaled, off the graphics thread)
  score each island
  hysteresis + fade
```

Auto-hide **off**: no analysis, no extra CPU. The source is a static cut-out.

Presence matches the **slot/frame**, not icons, numbers, or minimap terrain.

## Highlight cleanup (setup only)

User stroke = foreground seed, inside a dilated box of the paint:

1. GrabCut / watershed / edge snap (spike one, keep it if it hugs chrome).
2. Morphology + feather.
3. Connected-component islands.
4. Crop = bounding box of remaining alpha.

Runs on a snapshot or paused frame when the user asks, never at 60 fps.

## Persistence

On the source:

- Target source name/uuid
- Crop rect
- Mask (and island list)
- Per-island presence signatures
- Feather, invert, auto-hide on/off

## Performance budget

All HUD Mask instances combined should cost less than an extra game capture:

- GPU: sample + small masked blit
- CPU: only if auto-hide is on; per-island crops, ≤10 Hz, never full-frame
- Cleanup: editor only

If presence cannot stay in that budget, it stays off and the cut-out still works.

## Stream Suite

Normal OBS source. Intended placement: extra (vertical) canvas, sampling a main-canvas capture. Must also work on a single vanilla canvas. Do not link Stream Suite.

## Safety

- No injection, game memory, network, or telemetry
- Auto-hide off by default
- Scene-item visibility still overrides

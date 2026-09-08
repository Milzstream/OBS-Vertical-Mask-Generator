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

The normal properties sheet is small: sampled source, **Edit cutout…**, feather, invert, **Auto-hide** checkbox. If cleanup found more than one island, **Split into N sources**.

## Mask islands

After cleanup, the mask is labeled into **connected components** (islands). A highlight over three separate ability circles should yield three islands. A single solid panel is one island.

Each island stores:

- Pixel mask
- Bounding box (in the crop)
- Presence signature (chrome, not fill)

The scene item’s size stays the **full crop**. Hidden islands become transparent holes; the item does not jump on the canvas.

If cleanup merges blobs that should stay separate, the user either tightens the highlight or uses **Split into N sources**.

### Split into N sources

Setup-time only. If the cleaned mask has 3 islands, the editor can offer one click: create three HUD Mask sources (same sampled target, one island each), place them so the composite still lines up, remove the original combined source. Each can then hide, move, and scale on its own without highlighting the bar three times.

That is the preferred way to get independent control. It costs nothing at stream time.

## Runtime

```
Every frame (GPU), per visible HUD Mask
  sample target into a texrender
  draw crop through the current mask
  (optional) zero alpha on islands scored absent
```

### Shared presence cycle (not per source)

Auto-hide is **one plugin-wide pass**, not a timer on each mask.

```
Every ~100–200 ms, if any visible HUD Mask has auto-hide on:

  1. Collect instances that are
     - auto-hide enabled
     - visible as a scene item on an active canvas
  2. Group them by sampled target (minimap + abilities on Game Capture = one group)
  3. For each unique target: one downsample / readback
  4. Score every island in that group from the same buffer
  5. Push present/absent back to each instance (hysteresis + fade)
```

Hidden items, inactive scenes, and auto-hide-off sources are skipped. Adding a fifth mask that samples the same game capture should not add another GPU readback.

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

All HUD Mask instances combined should cost less than an extra game capture.

- **Draw:** GPU sample + small masked blit per visible instance (same order as today’s clones).
- **Auto-hide off:** no analysis.
- **Auto-hide on:** one cycle for the whole plugin. GPU cost scales with **unique sampled targets that are visible**, not with mask count. CPU scores small ROIs from that buffer. ≤10 Hz. Nothing on the graphics hot path. No extra full-res 4K readback per mask.
- **Cleanup / split:** editor only.

If presence cannot stay in that budget, it stays off and the cut-out still works.

## Stream Suite

Normal OBS source. Intended placement: extra (vertical) canvas, sampling a main-canvas capture. Must also work on a single vanilla canvas. Do not link Stream Suite.

## Safety

- No injection, game memory, network, or telemetry
- Auto-hide off by default
- Scene-item visibility still overrides

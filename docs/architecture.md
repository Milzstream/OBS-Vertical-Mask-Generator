# Architecture

## Form factor

OBS plugin (C++/Qt), new **input source**. GPL-2.0-or-later.

| Choice | Why |
| --- | --- |
| Source, not filter | Needs its own scene item to drag and scale. |
| Sample any source | Game capture, scene, display, browser — same OBS API. |
| No game database | Settings persist on the source in the scene collection. |
| Custom Qt editor | Stock properties cannot host a live video + highlighter. |
| Windows first | CI publishes a Windows installer, portable zip, and source zip. macOS/Linux are later. |

## Add-source flow

```
Add Source → HUD Mask → name it
        ↓
Properties: Type, Canvas (if Scene), target, Same Masks, Draw mask…, Expand, Feather
        ↓
Draw mask editor
  live (or paused) view of the sampled source
  Mask / Erase · Circle · Square · Line · Fill · Magic Select
  Snap Edges · Undo · Refresh · Clear
  Apply
        ↓
source size = opaque bbox of the mask plus 32px pad
user transforms the scene item
```

The properties sheet is small. Crop insets and the mask PNG path are stored on the source but not shown. There is no invert control and no Split into N.

## Runtime

```
Every frame (GPU), per visible HUD Mask
  sample target into a texrender
  draw the crop through the current mask (expand / feather already baked or applied)
```

No presence analysis. Auto-hide is stored as `auto_hide` (default off) and is not used yet. See [#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20).

## Highlight tools (setup only)

User paint is the mask. Optional **Snap Edges** walks the painted border and pulls it onto nearby frame contrast (any color). **Magic Select** shrinkwraps a rough loop onto the outer edge. **Fill** floods an outline and swallows the ring. None of this runs at 60 fps.

Crop = bounding box of remaining opaque pixels, padded 32px. Mask PNG is saved at crop size under the plugin config directory.

## Persistence

On the source:

- Target kind, canvas uuid, source name
- Crop rect
- Mask PNG path
- Expand, feather
- `auto_hide` (unused)

## Performance budget

All HUD Mask instances combined should cost less than an extra game capture.

- **Draw:** GPU sample + small masked blit per visible instance (same order as a source clone + crop).
- **Auto-hide off (always, today):** no analysis.
- **Editor tools:** snapshot or paused frame only.

## Stream Suite

Normal OBS source. Intended placement: extra (vertical) canvas, sampling a main-canvas capture. Must also work on a single vanilla canvas. Do not link Stream Suite.

## Safety

- No injection, game memory, or telemetry
- GitHub `/releases/latest` only, for the optional update prompt
- Auto-hide will be off by default when it ships
- Scene-item visibility still overrides

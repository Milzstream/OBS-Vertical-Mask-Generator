# Feasibility

## Does this already exist?

No. See [prior-art.md](prior-art.md). Sampling and PNG masks are solved. The missing product is highlight → cleanup → placeable source, with optional presence.

## Straightforward (shipped)

| Piece | Notes |
| --- | --- |
| Sample any OBS source | Same idea as Source Clone |
| Live preview + highlighter | Custom Qt dialog |
| Crop + masked draw | GPU, cheap |
| Persist mask on the source | Scene collection settings |

## Highlight cleanup (shipped)

Paint, line, fill, Magic Select, and Snap Edges. Realistic for high-contrast chrome (circular minimap, ability rings, a dark meter panel). Weaker on translucent HUD, icons that look like the world, and soft glows. Escape hatch: erase and paint again. First-stroke outlines will not match a hand-traced PNG.

## Auto-hide (not shipped)

Optional checkbox, off by default — zero analysis cost when off.

**Whole instance first.** If the element is gone (loading screen, hide-UI key), the source is fully transparent. That is the easy win.

**Per island later.** Separate blobs in one mask hiding independently is a follow-up, not required for the first auto-hide.

Presence should be **one shared cycle** for all visible, auto-hide-on masks. Cost must not grow linearly with mask count. See [#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20).

Auto-hide will never be 100%. That is why it is a checkbox. With it off, the plugin still replaces Photoshop + crop filters.

# OBS Vertical Mask Generator

An OBS Studio plugin that cuts a piece out of any source (game capture, scene, display capture, browser, …) so it can be placed and scaled on its own.

It is a normal OBS source. The usual use is **vertical streaming**: crop the 16:9 world into a 9:16 canvas, then cut out HUD that the crop hid (minimap, action bars, meters, abilities) and park those pieces in the leftover space. It also works on a single canvas for any other cut-out.

**Free software** (GPL-2.0-or-later). No installer yet — the plugin is still in planning.

## How it works

1. **Add Source → HUD Mask.**
2. Pick the source to sample.
3. On a live view, roughly **highlight** the element.
4. The plugin **cleans up** the highlight so the mask hugs that element, then crops to it.
5. Place and scale the scene item yourself. The plugin does not auto-layout the canvas.
6. Optional: **Auto-hide** (off by default). When that element is gone from the sampled source, this instance goes transparent until it comes back.

No Photoshop masks, no crop-filter stack, no per-game database.

A highlight that covers several separate blobs (for example three ability circles) is stored as **islands**. Auto-hide can drop one island and leave the others. The editor can also **split** those islands into separate HUD Mask sources in one click. Auto-hide, when enabled, runs as **one check cycle** for all visible masks that share a sampled source — it does not add a timer per mask.

## Docs

| Doc | Contents |
| --- | --- |
| [docs/vision.md](docs/vision.md) | Product, success bar, non-goals |
| [docs/architecture.md](docs/architecture.md) | Source, editor, islands, presence |
| [docs/feasibility.md](docs/feasibility.md) | What is realistic, including auto-hide |
| [docs/manual-workflow.md](docs/manual-workflow.md) | The clone + PNG mask stack this replaces |
| [docs/prior-art.md](docs/prior-art.md) | Existing plugins and why this is not a duplicate |
| [docs/roadmap.md](docs/roadmap.md) | MVP → v1 → later |

## Requirements

- OBS Studio 32+
- Developed against Windows + Aitum Stream Suite; CI will also emit macOS/Linux packages
- GPL-2.0-or-later (required to link libobs). No telemetry, no paid tier

## Status

Planning. `src/` is empty until implementation starts from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). Issues: [github.com/Milzstream/OBS-Vertical-Mask-Generator/issues](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues).

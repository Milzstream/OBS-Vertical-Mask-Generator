# OBS Vertical Mask Generator

An OBS Studio plugin that **cuts out an element** from any source (game capture, scene, browser, display capture, …) so you can place that piece anywhere.

The reason it exists: Aitum Vertical / Stream Suite. Horizontal games do not fit a phone, so you crop the world into 9:16 and need the HUD that just got chopped off — minimap, action bars, meters, abilities — as separate scene items. Technically it is not Aitum-specific and not vertical-specific. It is a normal OBS source.

This repo is private and in planning. There is no installer yet. It will go **public when v1 is stable**. The plugin will be **free to use** (GPL-2.0-or-later).

## Intended use

1. On the Aitum Vertical (Stream Suite) scene, **Add Source → HUD Mask**.
2. Pick the source to sample.
3. On a **live view**, highlight the UI with a rough highlighter.
4. The plugin cleans the highlight so it hugs the element, and crops to it.
5. Drag / scale the cut-out on the canvas yourself. It does not auto-layout.
6. If that UI disappears (loading, vehicle, hidden HUD, cutscene), the source hides until it comes back.

One source per element. No per-game database. No Photoshop. No crop filters. No “walk around so we can detect UI.”

NTE and WoW are examples of the same job, not special cases.

## Docs

| Doc | What it covers |
| --- | --- |
| [docs/vision.md](docs/vision.md) | Product, success bar, non-goals |
| [docs/current-workflow.md](docs/current-workflow.md) | How this is done today (the stack to delete) |
| [docs/architecture.md](docs/architecture.md) | Source, cutout editor, render, presence |
| [docs/feasibility.md](docs/feasibility.md) | What we can and will not do |
| [docs/prior-art.md](docs/prior-art.md) | What already exists (spoiler: not this) |
| [docs/roadmap.md](docs/roadmap.md) | MVP (cut-out) → v1 (auto-hide, public) |

## Target

- Dogfood: Windows, OBS Studio 32+, Aitum Stream Suite
- CI: Windows installer + plugin zip + source zip, plus template macOS/Linux packages
- License: GPL-2.0-or-later, no telemetry, no paid tier

## Repo layout

```
docs/          product docs
src/           OBS plugin (not started; obs-plugintemplate)
data/locale/   HUD Mask strings
.github/       issue templates
```

## Issues

[github.com/Milzstream/OBS-Vertical-Mask-Generator/issues](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues)

| Phase | Issues |
| --- | --- |
| Epic | [#1](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/1) |
| MVP cut-out | [#2](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/2) bootstrap, [#3](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/3) sample/draw, [#4](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/4) + [#10](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/10) highlighter editor, [#5](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/5) Stream Suite, [#14](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/14) license |
| v1 public | [#7](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/7) auto-hide, [#8](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/8) per-element hide |
| After v1 | [#12](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/12) mask that follows resize |
| Research | [#15](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/15) prior art (this product is not already a plugin) |
| Not doing | [#11](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/11) motion detect, [#6](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/6) Photoshop PNG import as the product, [#9](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/9) game database, [#13](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/13) unsupervised full-frame find |

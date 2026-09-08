# OBS Vertical Mask Generator

An OBS Studio plugin that extracts HUD / UI pieces from a horizontal game source and places them on an Aitum Vertical (Stream Suite) canvas — without the current clone → crop → handmade PNG mask stack, and without leaving empty holes when the HUD is gone.

This repo is in the planning / scaffolding stage. Plugin code has not been written yet. Product intent, feasibility, and the first GitHub issues live in `docs/`.

## What this is for

Vertical streams (TikTok, YouTube Shorts, Instagram) cannot show a full 16:9 HUD. The useful bits — minimap, abilities, health, ammo — sit in the corners of the horizontal capture and need to be cut out, masked, and re-laid on a 9:16 canvas.

Today that is done by hand in OBS + Photoshop. It looks great when the HUD is present and the character layout matches the PNG. It looks wrong on menus, loading screens, cutscenes, vehicles, and character-specific ability bars.

This plugin is meant to:

1. Replace the clone + crop + image-mask workflow with **one source**.
2. **Hide itself** when that HUD is not actually on screen.
3. Later, **help generate and adjust** masks instead of requiring a new Photoshop file per game / character.

## What it is not

- Not an Aitum fork and not a Stream Suite feature request. It is a normal OBS source you add to the vertical canvas.
- Not a game overlay injector and not a memory reader. It only looks at frames OBS already has.
- Not "select a source and magically find every UI element in every game" for v1. That is a research goal, not the first ship.

## Intended OBS object

A new source type named **HUD Mask**.

- Pick an existing source (usually the raw game capture / scene).
- Crop to one HUD element.
- Apply a mask (imported PNG first, generated later).
- Output only those pixels with alpha.
- Place, scale, and drag it on the Aitum Vertical canvas like any other source.
- Optionally fade to transparent when the HUD is not detected in that crop.

One source instance per HUD element (abilities, radar, health, …). Independent hide is required: in Neverness to Everness the minimap can stay while abilities disappear in a vehicle.

## Docs

| Doc | What it covers |
| --- | --- |
| [docs/vision.md](docs/vision.md) | Problem, product shape, success criteria |
| [docs/current-workflow.md](docs/current-workflow.md) | How masks are made and used today |
| [docs/architecture.md](docs/architecture.md) | OBS plugin shape, data model, render path |
| [docs/feasibility.md](docs/feasibility.md) | What we can, might, and cannot do |
| [docs/roadmap.md](docs/roadmap.md) | MVP → v1 → v2 phasing |

## Target environment (first)

- Windows
- OBS Studio 32+
- Aitum Stream Suite (vertical canvas)
- Existing PNG masks in `C:\Users\Milz\OneDrive\Streams\Vertical UI Masks` as the compatibility baseline

macOS / Linux are not a v1 goal.

**Free to use.** Licensed under GPL-2.0-or-later. That is the same family of license as OBS Studio; linking `libobs` requires a GPL-compatible license. There will be no paid tier, no license key, and no telemetry.

## Repo layout

```
docs/                 product + architecture (this phase)
profiles/             future game profiles (JSON + masks)
src/                  OBS plugin sources (not started)
data/locale/          OBS locale strings
.github/              issue templates
```

The C++ plugin will be bootstrapped from [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) when implementation starts. See GitHub issues.

## GitHub issues

Planning issues are on the private repo: [Milzstream/OBS-Vertical-Mask-Generator/issues](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues)

| # | Phase | Issue |
| --- | --- | --- |
| [1](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/1) | — | Epic / product brief |
| [2](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/2)–[6](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/6), [14](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/14) | MVP | Plugin loads; one source replaces clone+crop+PNG |
| [7](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/7)–[9](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/9) | v1 | Auto-hide when HUD is gone; profiles |
| [10](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/10) | v1.5 | Mark HUD, plugin snaps a mask |
| [11](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/11)–[12](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/12) | v2 | Movement assist; ability-count tracking |
| [13](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/13) | research | Full-frame autodetect (not promised) |

## Status

Planning. No plugin binary yet. Do not install this repo into OBS.

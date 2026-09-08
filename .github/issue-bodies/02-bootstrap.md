## Phase

MVP

## Goal

A loadable OBS 32 plugin named **HUD Mask**. Dogfood Windows. CI emits the usual plugin artifacts.

## In scope

- Bootstrap from [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
- Module id e.g. `vertical-hud-mask` (name can stay even though it is a general cut-out)
- Locale string `HUD Mask`
- GitHub Actions: Windows **installer exe** (drops into OBS plugins folder), **plugin zip**, **source zip**
- Template also builds macOS pkg + Linux deb (untested until we care)
- README build + install notes

## Out of scope

- Cutout editor, sampling, presence
- Requiring Stream Suite to load

## Acceptance

- [ ] DLL loads in OBS 32 on Windows
- [ ] **HUD Mask** appears under Add Source on the **main** canvas and on an Aitum Vertical canvas
- [ ] Transform works; no crash
- [ ] CI on a tag would produce exe + zip + source archive

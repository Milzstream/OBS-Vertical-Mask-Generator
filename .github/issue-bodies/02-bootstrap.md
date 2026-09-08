## Phase

MVP

## Problem

The repo is docs-only. There is no loadable OBS plugin.

## Goal

A Windows OBS 32 plugin that loads, shows **HUD Mask** in the source list, and draws a placeholder (solid/transparent) so we can add it to an Aitum Vertical scene.

## In scope

- Bootstrap from [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
- CMake + `buildspec.json` targeting OBS 32
- Module id e.g. `vertical-hud-mask`
- Locale string `HUD Mask`
- Local Windows build instructions in README
- GPL-2.0-or-later (already chosen; required to link libobs)

## Out of scope

- Sampling another source
- Masking, crop, presence
- macOS / Linux CI as a ship gate
- Installer polish beyond what the template already generates

## Acceptance

- [ ] Plugin DLL loads in OBS 32 on Windows without errors in the log
- [ ] **HUD Mask** appears under Add Source
- [ ] Adding it to an Aitum Stream Suite vertical scene does not crash
- [ ] Source can be transformed (moved/scaled) like any other source
- [ ] README explains how to build

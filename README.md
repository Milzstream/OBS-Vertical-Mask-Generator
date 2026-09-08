# OBS Vertical Mask Generator

An OBS Studio plugin that cuts a piece out of any source (game capture, scene, display capture, browser, …) so it can be placed and scaled on its own.

It is a normal OBS source. The usual use is **vertical streaming**: crop the 16:9 world into a 9:16 canvas, then cut out HUD that the crop hid (minimap, action bars, meters, abilities) and park those pieces in the leftover space. It also works on a single canvas for any other cut-out.

**Free software** (GPL-2.0-or-later). Windows first.

## How it works

1. **Add Source → HUD Mask.**
2. Pick the source to sample.
3. **Draw mask…** on a live view and roughly highlight the element.
4. **Clean up shape**, then **Apply**. The plugin crops to that mask.
5. Place and scale the scene item yourself. The plugin does not auto-layout the canvas.

Optional **Expand** / **Feather** on the properties pane once a mask exists.

No Photoshop masks, no crop-filter stack, no per-game database.

## Install (Windows)

Close OBS first.

**Installer (recommended)** — run `vertical-hud-mask-<version>-windows-x64.exe` from [Releases](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/releases). It installs into `%ProgramData%\obs-studio\plugins\vertical-hud-mask\`.

**Portable zip** — unzip `vertical-hud-mask-<version>-windows-x64.zip` and copy the `vertical-hud-mask` folder into `%ProgramData%\obs-studio\plugins\`.

On the next OBS launch, **Add Source → HUD Mask**. If a newer GitHub release exists, the plugin asks whether to download it.

### Remove a manual Program Files copy

If you previously copied the DLL by hand, close OBS and run this from an **elevated** PowerShell so the installer is the only copy:

```powershell
Remove-Item -Force "C:\Program Files\obs-studio\obs-plugins\64bit\vertical-hud-mask.dll" -ErrorAction SilentlyContinue
Remove-Item -Force "C:\Program Files\obs-studio\obs-plugins\64bit\vertical-hud-mask.pdb" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "C:\Program Files\obs-studio\data\obs-plugins\vertical-hud-mask" -ErrorAction SilentlyContinue
```

Then install with the installer or the portable zip. Two copies (Program Files + ProgramData) would load the plugin twice.

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
- Windows 10/11 x64
- GPL-2.0-or-later (required to link libobs). No telemetry, no paid tier

## Status

**HUD Mask** can sample a source or scene, draw and clean up a mask, crop to it, and adjust expand/feather. Auto-hide is not implemented yet.

A tagged `x.y.z` push publishes a GitHub Release with the installer, portable zip, and source zip. The plugin checks that release list on OBS launch (public repository, no token).

Issues: [github.com/Milzstream/OBS-Vertical-Mask-Generator/issues](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues).

## Build (Windows)

Requires Visual Studio 2022, CMake 3.28+, and a network connection the first time (OBS SDK is downloaded). Set `HUD_MASK_SKIP_UPDATE=1` to disable the GitHub check while iterating.

```
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

CI on `main` and on version tags (`0.1.0`, `0.1.0-beta1`, `0.1.0-rc1`) produces:

- `vertical-hud-mask-<version>-windows-x64.exe` — installer
- `vertical-hud-mask-<version>-windows-x64.zip` — portable plugin folder
- `vertical-hud-mask-<version>-source.zip` — source archive

# HUD Mask

An [OBS Studio](https://obsproject.com/) plugin that cuts a piece out of any source so you can place and scale it on its own.

The usual use is **vertical streaming**: crop a 16:9 game into 9:16, then pull HUD the crop hid (minimap, bars, abilities) and park those pieces in the leftover space. It also works on a normal scene for any other cut-out. It is not tied to a specific game or to Aitum Vertical.

Free software (GPL-2.0-or-later). **Windows only** for now.

## Install

Requires OBS Studio 32+ on Windows 10/11 x64. Close OBS first.

1. Download the latest [release](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/releases).
2. Run the installer (`vertical-hud-mask-<version>-windows-x64.exe`).

   Or unzip the portable zip and copy the `vertical-hud-mask` folder into `%ProgramData%\obs-studio\plugins\`.
3. Start OBS → **Add Source → HUD Mask**.

On later launches, HUD Mask checks GitHub for a newer release and can open the download. Close OBS before running a new installer.

### If you copied files into Program Files by hand

Early builds were copied into the OBS install directory. That copy will load **in addition to** the installer and you will see HUD Mask twice. Close OBS, then from an **elevated** PowerShell:

```powershell
Remove-Item -Force "C:\Program Files\obs-studio\obs-plugins\64bit\vertical-hud-mask.dll" -ErrorAction SilentlyContinue
Remove-Item -Force "C:\Program Files\obs-studio\obs-plugins\64bit\vertical-hud-mask.pdb" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "C:\Program Files\obs-studio\data\obs-plugins\vertical-hud-mask" -ErrorAction SilentlyContinue
```

Then run the installer.

## Use

1. Add a **HUD Mask** source.
2. Set **Type** to Source or Scene and pick what to sample.
3. **Draw mask…** and roughly highlight the element (wheel = brush size, Ctrl+wheel zoom, Space or middle-mouse pan).
4. **Clean up shape**, then **Apply**.
5. Place and scale the scene item. The plugin does not auto-layout the canvas.

**Expand** and **Feather** on the properties pane are available after a mask exists.

Auto-hide (hide the cut-out when that HUD element is gone) is not in this release.

## Build from source

Visual Studio 2022, CMake 3.28+, and a network connection the first time (the OBS SDK is downloaded). Set `HUD_MASK_SKIP_UPDATE=1` to skip the GitHub check while iterating.

```
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

Tag `x.y.z` (for example `0.1.0`) on `main` to publish a GitHub Release with the installer, portable zip, and source zip.

## License

[GPL-2.0-or-later](LICENSE). Required to link libobs. No telemetry, no paid tier.

Design notes (how this was scoped): [vision](docs/vision.md) · [architecture](docs/architecture.md) · [roadmap](docs/roadmap.md)

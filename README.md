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

The installer writes only under ProgramData. On later launches, HUD Mask checks GitHub for a newer release and can open the download. Close OBS before running a new installer.

### If you copied files into Program Files by hand

Early local copies (and an old helper script) also dropped a DLL next to OBS. That copy loads **in addition to** the installer and you will see HUD Mask twice, or `Source 'vertical_hud_mask' already exists!`. Close OBS, then from an **elevated** PowerShell:

```powershell
Remove-Item -Force "C:\Program Files\obs-studio\obs-plugins\64bit\vertical-hud-mask.dll" -ErrorAction SilentlyContinue
Remove-Item -Force "C:\Program Files\obs-studio\obs-plugins\64bit\vertical-hud-mask.pdb" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "C:\Program Files\obs-studio\data\obs-plugins\vertical-hud-mask" -ErrorAction SilentlyContinue
```

Then run the installer (it also tries to remove those leftovers).

## Use

1. Add a **HUD Mask** source.
2. Set **Type** to Source or Scene. For a scene, pick the **Canvas** first (main or an extra canvas such as Aitum Vertical), then the scene.
3. Pick the source or scene to sample. **Same Masks** lists other HUD Masks already aimed at that target.
4. **Draw mask…**
   - **Mask Brush** / **Erase Brush**, then Circle or Square. Wheel or the Size slider sets the brush. Size is only for those two shapes.
   - **Line** — hold to place, pause to straighten, click near the start or press Enter to close.
   - **Fill** — click inside an outline (any brush or a closed line).
   - **Magic Select** — draw a rough loop around the object and release.
   - **Snap Edges** pulls a painted silhouette onto nearby contrast in the frame.
   - Ctrl+wheel or the zoom slider to zoom, Ctrl+0 for 100%. Space or middle-mouse to pan. Pause/Play freezes or resumes the live frame. Undo, Refresh, Clear, then **Apply**.
5. Place and scale the scene item. The plugin does not auto-layout the canvas.

**Expand** and **Feather** on the properties pane are available after a mask exists.

Auto-hide (hide the cut-out when that HUD element is gone) is **not in this release**. It is planned as an optional checkbox, off by default ([#20](https://github.com/Milzstream/OBS-Vertical-Mask-Generator/issues/20)).

## Build from source

Visual Studio 2022, CMake 3.28+, and a network connection the first time (the OBS SDK is downloaded). Set `HUD_MASK_SKIP_UPDATE=1` to skip the GitHub check while iterating.

```
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

Tag `x.y.z` (for example `0.1.0`) on `main` to publish a GitHub Release with the installer, portable zip, and source zip. CI runs the same `ctest` command on the Windows job.

## License

[GPL-2.0-or-later](LICENSE). Required to link libobs. No telemetry, no paid tier.

Design notes: [vision](docs/vision.md) · [architecture](docs/architecture.md) · [roadmap](docs/roadmap.md)

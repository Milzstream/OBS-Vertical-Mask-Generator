# Agent notes

Windows-only OBS plugin (C++/Qt). GPL-2.0-or-later. No telemetry, no game memory, no paid tier.

Read `docs/vision.md` (especially non-goals), `docs/architecture.md`, `docs/roadmap.md`, and `src/README.md` before changing behavior. Do not pick up roadmap items unless asked.

## Names (do not rename)

Existing scenes and saved masks depend on these. The display name is Source Cutout. Leave the rest alone, including copyright banners that still say `HUD Mask`.

| What | Value |
| --- | --- |
| Plugin folder / `buildspec.json` `name` | `vertical-hud-mask` |
| Source id | `vertical_hud_mask` |
| C++ type and prefixes | `hud_mask` |
| Locale keys | `HUDMask.*` in `data/locale/en-US.ini` |

## Where code goes

- OBS source, properties, GPU draw: `src/hud-mask.cpp`
- Draw-mask Qt editor: `src/cutout-editor.cpp`. It is already large. Do not put pure image math there.
- Mask math with no OBS or Qt: `src/mask-process.cpp`, plus a case in `tests/mask-process-test.cpp`. Version and GitHub JSON helpers stay in `src/update-parse.hpp` and are tested from that same file.
- MobileSAM: `src/magic-sam.cpp`. Missing DLL or models return `Unavailable`. The editor then edge-snaps with `mask_magic_shrinkwrap`. `Rejected` or `Failed` is a miss, not a fallback. Do not train or replace the decoder. The plan is a classical Magic Select; see `docs/roadmap.md`.
- Auto-hide cycle: `src/presence.cpp`
- Update check: `src/update-check.cpp`. Set `HUD_MASK_SKIP_UPDATE=1` while iterating.

Do not add a top-level file or directory unless `.gitignore` whitelists it. That file ignores `/*` and then un-ignores named paths. Do not edit `build_x64/`, `.deps/`, or fetched `onnxruntime.dll`.

## Build and test

Visual Studio 2022, CMake 3.28+, from the repo root:

```
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

Magic Select's DLL, once, then rebuild: `powershell -File .github/scripts/Fetch-MobileSAM.ps1`. Models ship in `data/models/`.

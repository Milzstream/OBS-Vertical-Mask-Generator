# Plugin sources

C++ OBS plugin bootstrapped from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

| File | Role |
| --- | --- |
| `plugin-main.cpp` | Module load/unload |
| `hud-mask.cpp` | HUD Mask source: sample, crop, mask, expand/feather |
| `cutout-editor.cpp` | Draw-mask editor (Qt) |
| `mask-process.cpp` | Expand, feather, flood fill/erase, snap, magic shrinkwrap, crop bbox (no OBS/Qt) |
| `presence.cpp` | Auto-hide shared cycle and still match |
| `update-check.cpp` | GitHub release check on OBS launch |
| `update-parse.hpp` | Version and GitHub JSON helpers (no OBS/Qt) |
| `plugin-support.*` | Logging helper from the template |

Properties: Type (source/scene), canvas when Type is Scene, target list, Same Masks, **Draw mask...**, Expand, Feather, Auto-hide, Fade, Match.

The editor captures a frame. Tools: Mask/Erase with Circle or Square, Line, Fill, Magic Select, Snap Edges, Undo, Pause/Play, zoom/pan. Apply crops to the opaque bbox (32px pad) and writes a PNG under OBS plugin config (`masks/<source-uuid>.png`). Crop and mask path stay in settings but are hidden on the properties sheet.

Auto-hide compares the processed mask (expand / feather) to the Draw mask still (`masks/<uuid>.ref`) with a small search for drift. Fade and Match sliders sit next to the checkbox. `presence.cpp` runs the shared downsample cycle (round-robin per target).

After OBS finishes loading, the plugin checks the public GitHub latest release and can open the download.

# Plugin sources

C++ OBS plugin bootstrapped from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

| File | Role |
| --- | --- |
| `plugin-main.cpp` | Module load/unload |
| `hud-mask.cpp` | Source Cutout source: sample, crop, mask, expand/feather |
| `cutout-editor.cpp` | Draw-mask editor (Qt) |
| `mask-process.cpp` | Expand, feather, flood fill/erase, snap, classical Magic Select pick, crop bbox (no OBS/Qt) |
| `magic-sam.cpp` | Unused by Magic Select. MobileSAM remains until the ONNX stack is removed |
| `presence.cpp` | Auto-hide shared cycle and still match |
| `update-check.cpp` | GitHub release check on OBS launch |
| `update-parse.hpp` | Version and GitHub JSON helpers (no OBS/Qt) |
| `plugin-support.*` | Logging helper from the template |

Properties: Type (source/scene), canvas when Type is Scene, target list, Same Masks, **Draw mask...**, Expand, Feather, Auto-hide, Mark show, Mark hide, Fade, Match.

The editor captures a frame. Tools: Mask/Erase with Circle or Square, Line, Shape, Fill, Magic Select, Snap Edges, Undo/Redo (32 steps, Ctrl+Z / Ctrl+Y), Pause/Play, zoom/pan. Magic Select calls `mask_magic_pick` off the UI thread. A miss leaves the mask untouched. Snap Edges is a separate tool. Apply crops to the opaque bbox (32px pad) and writes `masks/<source-uuid>.png` under OBS plugin config. Crop and mask path stay in settings but are hidden on the properties sheet. Draw scales those insets to the live filtered size.

Auto-hide compares the processed mask (expand / feather) to the Draw mask still (`masks/<uuid>.ref`) with a small search for drift. Fade and Match sliders sit next to the checkbox. Mark show and Mark hide save extra crops in that same file (at most 8). A hide match wins. The Apply still is never replaced. `presence.cpp` runs the shared downsample cycle (round-robin per target).

After OBS finishes loading, the plugin checks the public GitHub latest release and can download and start the Windows installer.

# Plugin sources

C++ OBS plugin bootstrapped from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

| File | Role |
| --- | --- |
| `plugin-main.cpp` | Module load/unload |
| `hud-mask.cpp` | HUD Mask source: sample, crop, mask, expand/feather |
| `cutout-editor.cpp` | Draw-mask editor |
| `mask-process.cpp` | Expand / feather / roundness helpers (no OBS/Qt) |
| `update-check.cpp` | GitHub release check on OBS launch |
| `plugin-support.*` | Logging helper from the template |

Properties: Type (source/scene), name list, **Draw mask...**, Expand, Feather.

The editor captures a frame, you highlight the UI (soft brush, erase, wheel for size), Apply crops and writes a PNG under OBS plugin config. Crop/mask path stay in settings but are not shown in the properties sheet. Auto-hide is not implemented yet. After OBS finishes loading, the plugin checks the public GitHub latest release and can open the download.

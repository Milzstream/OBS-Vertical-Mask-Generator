# Plugin sources

C++ OBS plugin bootstrapped from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

| File | Role |
| --- | --- |
| `plugin-main.cpp` | Module load/unload |
| `hud-mask.cpp` | HUD Mask source: sample, crop, optional PNG mask |
| `plugin-support.*` | Logging helper from the template |

Properties: Type (source/scene), name list, **Draw mask...**

The editor captures a frame, you highlight the UI (soft brush, erase, wheel for size), Apply crops and writes a PNG under OBS plugin config. Crop/mask path stay in settings but are not shown in the properties sheet. Auto-hide is not implemented yet.

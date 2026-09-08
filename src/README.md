# Plugin sources

C++ OBS plugin bootstrapped from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

| File | Role |
| --- | --- |
| `plugin-main.cpp` | Module load/unload |
| `hud-mask.cpp` | HUD Mask source: sample, crop, optional PNG mask |
| `plugin-support.*` | Logging helper from the template |

The cutout editor (highlighter) is not in this tree yet. Current properties: source picker, crop insets, mask PNG path. Auto-hide is shown but disabled.

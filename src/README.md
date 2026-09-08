# Plugin sources

Empty until implementation. Bootstrap from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

- OBS Studio 32; test Windows first; CI also emits macOS/Linux packages
- Display name: **HUD Mask**
- `OBS_SOURCE_TYPE_INPUT`, custom draw, video only
- First useful UI: cutout editor (live preview + highlighter)
- Auto-hide is a checkbox, off by default; no analysis on the render thread
- No game database

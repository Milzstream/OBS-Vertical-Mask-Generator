# Plugin sources

Empty on purpose. Implementation starts after the revised GitHub MVP issues.

When work begins:

1. Bootstrap from https://github.com/obsproject/obs-plugintemplate
2. OBS Studio 32; dogfood Windows; CI also emits macOS/Linux packages
3. Display name: **HUD Mask**
4. `OBS_SOURCE_TYPE_INPUT`, custom draw, video only
5. First useful UI is the **cutout editor** (live preview + highlighter), not crop spinboxes
6. Do not add a game database, motion calibration, or CV on the render thread in the first PR

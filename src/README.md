# Plugin sources

Empty on purpose. C++ implementation starts after the GitHub MVP issues are agreed.

When work begins:

1. Bootstrap from https://github.com/obsproject/obs-plugintemplate
2. Target OBS Studio 32, Windows, 64-bit
3. Module display name: **HUD Mask**
4. Register an `OBS_SOURCE_TYPE_INPUT` with custom draw and video (no audio)

Do not add a filter, dock, or CV library in the first PR. Source + crop + PNG mask first.

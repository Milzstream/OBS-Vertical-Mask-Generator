# Architecture

## Form factor

**OBS plugin (C++), new input source**, not a filter and not a standalone app.

| Choice | Why |
| --- | --- |
| Source, not filter | The vertical canvas needs its own scene item that can be moved/scaled. A filter still needs a clone to hang off. A source *is* that item. |
| Sample another source | Same idea as Exeldro Source Clone: `obs_source_video_render` the chosen source into a `gs_texrender`, then crop and mask. |
| No Aitum API | Stream Suite's vertical canvas is a normal OBS canvas. A registered `obs_source_info` can be added there. Depending on Stream Suite internals would break on their updates. |
| One instance per HUD element | Independent show/hide (NTE abilities vs radar). Matches how the scene is already built. |
| Windows / OBS 32 first | That is the Stream Suite environment in use. |

A filter can be added later if someone wants to mask an existing clone. It is not required for the first useful version.

## Plugin bootstrap

Implementation starts from [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate):

- CMake + `buildspec.json` for OBS 32
- GitHub Actions for Windows (macOS/Linux CI optional, not a v1 ship gate)
- Module id something like `vertical-hud-mask`
- Display name **HUD Mask**

GPL-2.0-or-later, same as libobs.

## Runtime pieces

```
┌─────────────────────────────────────────────┐
│ HUD Mask source (on Aitum Vertical scene)   │
│                                             │
│  1. Resolve target source by name           │
│  2. Render it to a texrender (GPU)          │
│  3. Draw the crop rect through a mask       │
│     shader (PNG or generated texture)       │
│  4. Multiply RGB by presence alpha          │
│  5. Output as this source's video           │
└─────────────────────────────────────────────┘
                 ▲
                 │ throttled GPU→CPU readback of the crop only
                 │
┌─────────────────────────────────────────────┐
│ Presence worker (not on the graphics hot    │
│ path)                                       │
│  - probe / template score at ~4–10 Hz       │
│  - hysteresis + fade                        │
└─────────────────────────────────────────────┘
```

### Render path (every frame, GPU, cheap)

1. If the target source is missing, draw nothing.
2. Texrender the target (optionally without its filters — should be a property, default "with filters" off for a raw game capture).
3. Set ortho to the crop rectangle.
4. Sample the mask texture; `rgb = game; a = mask.r * presence`.
5. Feather is either baked into the PNG (current art) or a shader blur of the mask.

Do **not** read the full 4K frame back to the CPU on the graphics thread.

### Presence path (few times per second, small ROI)

Read back only the cropped region, possibly downscaled (e.g. 64–128 px on the long side).

v1 modes, in order of implementation:

1. **Manual** — user hides the source. Same as today.
2. **Probe** — N sample points chosen during calibration (HUD chrome colors / edges). Score vs a stored reference.
3. **Template** — small grayscale snippet of the HUD chrome (not the world-filled interior of a minimap). Normalized correlation or mean absolute error.

Hysteresis: require several consecutive low scores before hide, several high scores before show. Fade opacity over ~150–300 ms so it does not pop.

Presence is **per source instance**. Abilities can hide while radar stays.

## Data model

A **profile** is a named game (or game + character) with one or more **slots**.

```json
{
  "schemaVersion": 1,
  "game": "NTE",
  "variant": "default",
  "sourceHint": "Game Capture",
  "canvas": { "width": 2560, "height": 1440 },
  "slots": [
    {
      "id": "radar",
      "label": "Radar",
      "crop": { "left": 45, "top": 10, "right": 2180, "bottom": 1065 },
      "mask": { "type": "png", "file": "radar.png" },
      "presence": {
        "mode": "template",
        "hideThreshold": 0.35,
        "showThreshold": 0.55,
        "holdMs": 250
      }
    }
  ]
}
```

`crop` uses OBS-style insets from each edge of the **sampled source**, matching the Creating Masks.png notes.

OBS scene collection stores the live source settings (target source name, crop, mask path, presence params). Profiles are a convenience pack that can stamp those settings onto new HUD Mask sources.

Schema lives in [`profiles/schema.json`](../profiles/schema.json).

## UI

### v1 — source properties

- Source picker (existing sources)
- Crop left/top/right/bottom
- Mask PNG path (with a preview)
- Feather
- Presence enable + sensitivity
- "Capture reference" button (stores the template / probes from the current crop)

### v1.5 — calibration dock (optional, high value)

- Snapshot of the sampled source
- Roughly **circle or box** HUD pieces (does not need to be pixel-perfect)
- Plugin edge-snaps the mark into a crop + mask
- Optional **“look around for 2 seconds”** pass that highlights stable pixels as HUD candidates
- Preview the masked result
- Tune presence on a live score meter

The dock is not required to prove the source works. It is required to make setup faster than Photoshop + crop math.

## Interaction with Aitum Stream Suite

- User adds **HUD Mask** on the vertical canvas.
- Target source is the horizontal game capture / scene.
- Source Clone is **not** required once HUD Mask can sample the original.
- Scene item transform, visibility, blend, and hotkeys are stock OBS.
- We do not hook Stream Suite docks, outputs, or scene linking.

Risk to test early: sampling a source that lives on another canvas (Stream Suite extra canvas). Exeldro Source Clone had bugs here and later fixed "cloning sources from extra canvas". Our sampling code should follow that pattern and be tested on Stream Suite, not only vanilla OBS.

## Safety

- No process injection, no reading another process's memory.
- No network.
- Analysis is optional and off by default until calibrated, so a mis-tuned detector cannot flicker a stream on first add.
- Mask PNGs are local files.

## Build / repo split

| Path | Role |
| --- | --- |
| `src/` | Plugin C++ (not started) |
| `data/` | Locale, default effects |
| `profiles/` | Example / user game packs |
| `docs/` | Product docs |
| `.github/` | Issues, later CI from the plugin template |

Personal mask PNGs and PSDs stay in OneDrive, not in git.

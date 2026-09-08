# Architecture

## Form factor

**OBS plugin (C++/Qt), new input source.** One instance = one cut-out.

| Choice | Why |
| --- | --- |
| Source, not filter | Needs its own scene item to drag and scale on any canvas. A filter still needs a clone to hang off. |
| Sample any source | Game capture, scene, display capture, browser — same OBS API. |
| No game database | Settings live on the source, which already persists in the OBS scene collection. |
| Custom Qt editor | Stock `obs_properties` cannot host a live video + highlighter. The add-source / properties flow opens our cutout window. |
| Windows dogfood, multi-platform build | OBS plugins from obs-plugintemplate already CI to Win installer + zip, macOS pkg, Linux deb, source archive. We test Windows first. |

GPL-2.0-or-later.

## Add-source flow (the product)

```
Add Source → HUD Mask → name it
        ↓
┌─────────────────────────────────────────┐
│  Sample: [ Game Capture          ▼ ]    │
│                                         │
│  ┌─────────────────────────────────┐    │
│  │  live view of sampled source    │    │
│  │  user highlighter over the UI   │    │
│  └─────────────────────────────────┘    │
│  tools: highlight · erase · reset       │
│  [ Cleanup ]  preview of tight mask     │
│                           [ OK ]        │
└─────────────────────────────────────────┘
        ↓
source size = bounding box of the cleaned mask
scene item appears on the canvas → user transforms it
```

OBS will still show the normal properties sheet. That sheet is: sampled-source dropdown, button **Edit cutout…** (reopens this window), maybe feather / invert. The highlighter is not a row of spinboxes.

## Runtime

```
Every frame (GPU, cheap)
  sample target source into a texrender
  draw the crop through the cleaned mask
  multiply alpha by presence (0..1)

Few times per second (small ROI, not full 4K, off the graphics thread)
  score: “is this UI element still here?”
  hysteresis → fade presence
```

Presence must ignore **contents**:

| Element | Interior changes | What we should match |
| --- | --- | --- |
| WoW action bar | Spell icons, cooldowns | Slot chrome / bar shape |
| WoW Details meter | Numbers, bar fill, height | Dark window, roughly |
| Minimap | Terrain, pings | Frame / circle, not the map |
| NTE abilities | Icons, which slots exist | Slot frames; hide if the group is gone |

If we template-match the highlighted pixels naively, the action bar will flicker every time a cooldown pulses. The stored signature should be biased toward stable chrome, or a loose match on the masked region as a whole.

## Highlight cleanup (setup, not every frame)

User stroke = “this is the UI” seed. Plugin, inside a dilated bounding box of the stroke:

1. Treat the stroke as foreground, unpainted as likely background.
2. GrabCut / watershed / edge snap — pick one spike and keep it if it hugs chrome.
3. Morphology (close holes, drop specks) + feather.
4. Crop = bounding box of remaining alpha.

“Perfectly match” is the aim. First versions will miss translucent edges and busy action bars. **Erase and paint again** is the escape hatch, not a game-specific tweak.

Cleanup runs when the user asks (button or pause after a stroke), not at 60 fps.

## Persistence

Saved on the source (scene collection JSON):

- Target source name/uuid
- Crop rect (computed)
- Mask texture (PNG in the source settings, or a file next to the scene collection)
- Presence signature (from the cleaned crop at OK time)
- Feather, invert, presence on/off

No `profiles/` game packs. That folder in this repo is leftover planning and can go away.

## Stream Suite and everywhere else

HUD Mask is a normal OBS source. It can sit on the main canvas, a vertical canvas, or any extra canvas. The intended use is: add it on Aitum Vertical, point it at a main-canvas game capture.

We do not link Stream Suite. Extra-canvas sampling is still the sharp test (Source Clone has burned itself on this before), but the plugin must also work in vanilla OBS with a single canvas.

## Packaging (CI)

obs-plugintemplate default artifacts, which match what you see on other plugins:

- Windows: installer `.exe` that puts the plugin in the OBS plugins folder, plus a zip of the same files
- Source: `.zip` / `.tar.xz` of the tagged tree
- macOS `.pkg` and Linux `.deb` from the same CI, untested until we care

v1 public release = a GitHub Release with those files and a short install note.

## Safety

- No process injection, no game memory, no network, no telemetry.
- Presence off until a cutout exists, so a new source does not flicker.
- Manual visibility still overrides.

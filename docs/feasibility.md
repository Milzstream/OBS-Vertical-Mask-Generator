# Feasibility

Honest split of what this plugin can do, what it might do with work, and what it should not promise.

## Can do (engineering, not research)

These are known OBS plugin patterns.

| Capability | Why it is tractable |
| --- | --- |
| New source type that draws with alpha | Standard `obs_source_info` + `OBS_SOURCE_CUSTOM_DRAW` |
| Sample another source into a texture | Same approach as Source Clone (`gs_texrender` + `obs_source_video_render`) |
| Crop | Ortho / sprite UV of a sub-rect. Same numbers as the current Crop filter |
| Apply a PNG mask with feathered edges | Image Mask/Blend already does this; we do it in our shader |
| Scene item transform on the vertical canvas | Free: any source can be moved/scaled on any canvas |
| Import existing `Vertical UI Masks` PNGs | Files are ordinary white-on-black masks |
| Multiple instances | One source per HUD slot |
| Windows + OBS 32 plugin package | obs-plugintemplate |

**MVP is this list.** If we only shipped this, the plugin would already collapse a 3-filter stack into one source. That is useful, but it does **not** yet solve bleed-through.

## Can do (the actual differentiator), with caveats

### Presence detection / auto-hide

**Feasible** if we constrain it.

HUD chrome is usually:

- Screen-space (does not move with the camera)
- High-contrast, repeated shapes (circles, bars, diamonds)
- Present or absent as a set, not randomly flickering every frame

A small template of the **chrome** (the ring of a minimap, the frames of ability slots — not the moving map/icons inside) compared against the current crop is a solved technique. Advanced Scene Switcher already does video-pattern conditions; we would do a tighter, per-source version.

What makes it work:

- User calibrates on a frame where the HUD is visible.
- We compare a tiny ROI, not 4K.
- Hysteresis + fade, so a one-frame ability animation does not hide the slot.
- Independent per slot.

What will still fail sometimes:

- HUD that **animates its chrome** (NTE ability slots pulsing, Destiny radar changing state).
- HUD that **stays on screen during cinematics** (some games keep a minimap).
- Lighting/color filters on the sampled source that were not there at calibration.
- A crop so tight that the template is mostly the world, not the chrome.

This will need per-game tuning. It will not be 100%. It can be **good enough that I do not have to hotkey-hide abilities every time I enter a menu or a car**.

### Character-specific ability layouts (NTE)

A **single static PNG** cannot cover every kit. Options that are feasible:

1. **Variants in a profile** (`NTE/abilities-3slot.png` vs `4slot`) and pick manually — easy, not magical.
2. **Best-template match** among a few stored variants — feasible.
3. **Detect the circles and generate the mask** — feasible for this specific shape (Hough / blob on a small crop), not as a general HUD solver.

(3) is the interesting one for NTE and should be a v2 experiment on **abilities only**, not the whole plugin's architecture.

### Assisted mask generation (preferred automation path)

Two user-guided methods are realistic. Full "scan the 16:9 frame and find every HUD" is not, as a first feature.

**A. User marks, plugin finishes (high confidence)**

1. Snapshot the sampled source.
2. User roughly circles / boxes each HUD piece (does not need to be pixel-perfect).
3. Plugin takes that smaller ROI and runs edge / flood / threshold to snap to the chrome.
4. Feather the result to match the current Photoshop look.
5. User accepts or nudges.

This is the right default. The hard part of HUD extraction is *which* pixels are UI; a scribble removes that ambiguity. Edge detection on a 300×300 crop is cheap and reliable for circles, bars, and diamonds.

**B. Request movement, then keep what did not move (medium confidence)**

HUD is screen-space. The world is not.

1. User clicks "calibrate".
2. Plugin asks them to look around / walk / turn the camera for ~2 seconds.
3. Pixels with **low variance** in that window are treated as UI; high-variance pixels are world.
4. Connected components in the stable map become candidate slots.
5. User confirms which blobs to keep (radar vs leftover static props).

This can propose slots with almost no drawing. It fails on menus (everything is static), cutscenes, and translucent HUD over a still sky. Use it as a *helper to find boxes*, then run (A) inside each box.

**C. Combine:** movement proposes regions → user taps the ones that are HUD → plugin edge-snaps a mask → presence template is captured from the same frame.

Do not start with a general HUD AI. Start with (A), add (B) when (A) is boring.

## Might do later (research, not a promise)

### "Select a source and find all the UI"

Fully automatic HUD discovery across arbitrary games is a research problem:

- Menus are 100% UI; the detector would "find" the whole screen.
- Loading screens and diegetic UI (holograms, in-world maps) confuse "static = HUD".
- Some HUDs are translucent and sit on similar colors.
- Games change HUD scale with settings / resolution / aspect.

A **region-limited** version ("in this box, find the stable shape") is much more realistic than "scan the whole 16:9 frame and propose every slot".

Do not schedule automatic full-frame discovery as a feature until presence + assisted generation work on the games we already mask.

### Live mask morphing every frame

Reshaping the mask 60 times a second to follow an animating health bar is expensive and easy to get wrong (the bar fill is supposed to be inside the mask; the mask should follow chrome, not fill). Prefer: hide, switch variant, or regenerate on a slow timer when circle *count* changes.

## Cannot / will not do

| Idea | Why not |
| --- | --- |
| Read game memory for "in menu" / "in vehicle" | Anti-cheat (especially Destiny). Also game-specific and brittle. |
| Inject a ReShade-style overlay into the game | Same risk; also not how OBS compositing should work |
| Depend on a private Aitum Stream Suite API | No supported public API for this; would break on their releases |
| Guarantee zero false hides | Presence is statistical. We ship hysteresis and a manual override |
| Build the entire vertical scene (gameplay crop, cam, alerts, chat) | Out of scope. Other docks already exist for that |
| Replace the branding overlay | Separate graphic |
| Support every capture method's color space perfectly on day one | HDR / canvas color space already bites Source Clone; we test SDR first |

## Performance budget (target)

Dual-canvas streaming is already heavy. Budget for *all* HUD Mask instances combined:

- GPU: a few extra texrenders of crops, not extra full-frame copies of the game.
- CPU: presence at ≤10 Hz, crop downscaled, off the graphics thread.
- No OpenCV on the render thread. If we use a CV library, it is for the worker and optional.

If presence cannot meet that, presence stays off and the source still works as a static masked crop.

## Compatibility notes

- **Source Clone** remains useful for other things; HUD Mask should not require it.
- **Stream Suite extra canvases:** must be in the test matrix. Sampling across canvases is the sharp edge.
- **Color space:** start with SDR game capture. HDR passthrough is a known footgun.
- **Existing PNGs:** must look the same as Image Mask/Blend with "Alpha Mask Blend" / white-on-black. Match that convention exactly so current art is reusable.

## Recommended stance

Build in this order, and only promote a layer when the one under it is boringly reliable:

1. Source + crop + PNG mask (parity with today).
2. Presence hide/show on NTE abilities + radar (the screenshots we have).
3. Calibration UI so I am not typing crop insets.
4. Assisted generation + circle-count tracking as experiments on NTE abilities.

Do not start with a general HUD AI.

# Feasibility

## Does a plugin like this already exist?

**No.** See [prior-art.md](prior-art.md). Vertical HUD guides in 2025–2026 still teach Source Clone + a handmade PNG. Advanced Masks / Visual Crop / Advanced Scene Switcher cover *pieces* (shapes, rectangle crop, pattern-match hide). None of them is “highlight an element on any source → cut it out → hide when it is gone.”

That absence is not a sign that OBS cannot do it. Sampling + masking is solved. The missing piece is the editor + presence packaged as one source.

## Can do

| Piece | Why |
| --- | --- |
| Sample any OBS source into a texture | Same pattern as Source Clone |
| Custom Qt window with a live preview | Common for docks / “open editor” buttons; not stock properties widgets |
| Highlighter (brush) on that preview | QPainter / overlay on a grabbed frame or a preview widget |
| Crop + masked draw every frame | GPU shader, cheap |
| Scene-item transform on any canvas | Free; any OBS source. Intended use is Aitum Vertical. |
| Windows installer + zip + source zip via GitHub Actions | obs-plugintemplate already does this; macOS/Linux packages come along for the ride |
| Persist mask on the source | OBS settings |

## Can do, with honesty

### Cleanup: rough highlight → tight mask

This is the setup feature. It is **not** magic “detect all UI.” You already told it where to look.

Inside the painted region, hugging a high-contrast panel (WoW Details, a circular minimap, NTE ability circles) is realistic: seed from the stroke, snap to edges, feather.

It will be worse on:

- Translucent HUD
- Action bars whose icons look like the world
- UI that sits on a similar color (dark meter on a dark cave)
- Soft glows / drop shadows

Escape hatch: erase, paint again, or accept “good enough on mobile.” Your WoW action bar is already not pixel-perfect and that is fine.

We should **not** promise Photoshop-identical outlines on the first stroke.

### Hide when the UI is gone

Feasible if we detect **the element**, not the artwork inside it.

Works well when the whole widget is missing: loading screen, `Alt+Z`, NTE vehicle eating the ability bar, character select.

Works poorly if we compare a screenshot of spell icons — those change every second. Signature should be “bar/panel still in this crop,” with hysteresis.

Damage meters that **resize** in combat may clip or leave a gap. v1 can hide/show; resizing the mask is after v1.

Will never be 100%. Manual hide still exists.

## Will not do

| Idea | Status |
| --- | --- |
| Walk around while we detect UI from motion | **Killed.** Bad setup UX; menus break it anyway. |
| In-plugin database of every game | **Killed.** Highlight is the mechanism. |
| Auto-place cut-outs on the vertical canvas | **Killed.** User drags. |
| Unsupervised full-frame HUD finder | Research at most; not v1. |
| Game memory / injection | Never. |
| Aitum-private API | Never. |

## Performance

All HUD Mask instances together:

- GPU: texrender of the sampled source (or a shared cache later) + small masked blit
- CPU: presence on the **crop**, downscaled, ≤10 Hz
- Cleanup: only in the editor, on a snapshot or paused live frame

## Platforms

OBS plugins can be cross-platform. We **build** all three via the template. We **test** Windows + OBS 32 + Stream Suite because that is the machine this is for. If a macOS/Linux artifact is broken at v1, that is a bug to file, not a release blocker.

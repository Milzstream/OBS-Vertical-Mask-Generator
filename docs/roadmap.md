# Roadmap

## Phase 0 — this repo (done)

Docs, issues, empty `src/`. No OBS binary.

## Phase MVP — cut-out exists

**Goal:** Add source → pick what to sample → highlight a UI element on a live view → plugin cleans the highlight and crops → I can drag that cut-out around the vertical canvas.

- Bootstrap from obs-plugintemplate (OBS 32)
- HUD Mask source that samples another source (scene / game capture / browser / …)
- **Custom cutout editor** in the add/properties flow: live preview + highlighter + cleanup + crop
- Transformable scene item on any OBS canvas (dogfood: Aitum Vertical)
- Windows is the dogfood platform; CI still produces the usual OBS plugin artifacts (installer, plugin zip, source zip) and template macOS/Linux packages
- Manual hide still works (stock visibility)

**Exit:** I can recreate the WoW vertical layout (meter, minimap, action bar as three HUD Mask sources) **without** Photoshop, crop filters, or source clones. Cleanup does not have to be perfect; I can re-highlight.

Existing PNG masks are **not** a requirement. They are a reference for how feathered cut-outs should look.

## Phase v1 — hide when the UI is gone (public)

**Goal:** The cut-out does not show world/loading/menu through a hole when that UI is missing. This is the “stable, share the repo” bar.

- Presence of the **element**, not its contents (action-bar icons change; the bar is still there)
- Per-instance hide (meter can hide, minimap can stay)
- Hysteresis + fade so it does not flicker
- Re-highlight from properties if I need to redo a mask
- Windows installer from CI that drops files into the OBS plugins folder

**Exit:** I can play WoW and NTE (and a third game we have never authored for) on vertical without toggling these sources by hand. Then the repo can go public.

## After v1

Only if v1 is boringly usable:

- Better cleanup (tighter hugs, eraser, multiple strokes)
- Mask that slowly updates if a panel resizes (Details meter growing in combat)
- macOS / Linux as tested platforms, not just CI artifacts

## Killed / not doing

- Motion / “walk around while we detect UI”
- A growing in-plugin database of games
- Full-frame “find every HUD with no highlight”
- Auto-layout of cut-outs on the vertical canvas
- Shipping Photoshop PNGs as the way to set up a game

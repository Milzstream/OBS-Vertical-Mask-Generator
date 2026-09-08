# Vision

## The job to be done

Horizontal games are 16:9. Vertical streams are 9:16. You cannot rotate the game to fill the phone; that is a bad viewer experience. The usual fix is:

1. Crop the world so it sits in the middle of the vertical canvas.
2. **Cut out** the UI that the crop just hid (minimap, action bars, meters, abilities, …).
3. Place those cut-outs in the leftover vertical space (top, bottom, corners).
4. Scale them if needed.

That is a **per-element cut-out**, not a per-game library. NTE, WoW, Destiny, and whatever ships next year are all the same job.

### WoW example (screenshot)

On the main 16:9 canvas the UI lives in the usual places (action bars along the bottom, Details meter as a dark box, minimap, player frames, chat). The vertical canvas is a center crop of the world plus:

- Damage meter cut out and moved to the **top-left**
- Minimap cut out and moved to the **top-right**
- Action bar cut out, **scaled** to the vertical width, and parked at the bottom

The plugin does **not** decide those positions. The user drags each cut-out around the canvas. The plugin only produces the cut-out, sizes the source to it, and later hides it when that UI is gone.

## What I do today (the thing to delete)

For each UI piece:

1. Source-clone the game.
2. Crop filter, just larger than the element.
3. Photoshop a white-on-black PNG.
4. Image Mask/Blend.
5. Transform on Aitum Vertical.
6. Manually hide when loading screens, vehicles, `Alt+Z`, cutscenes, etc. remove that UI.

The `Vertical UI Masks` folders are a symptom of this. When the plugin works, those files should be unnecessary.

## What we are building

An OBS **source** (working name **HUD Mask** / **UI Cutout**).

It is a **general OBS cut-out source**. The reason it exists is Aitum Vertical HUD, but it is not Aitum-specific and not vertical-specific. Anyone can add it to any scene on any canvas and cut a piece out of any other source.

When you add it to a scene (for me: the vertical canvas):

1. Pick any existing source to sample — scene, game capture, display capture, browser, …
2. That dialog shows a **live view** of the sampled source.
3. You **highlight** the UI you want to cut out (a highlighter / brush, rough is fine).
4. The plugin **cleans up** the highlight so the mask hugs the element, then **crops** to that mask.
5. You OK out. The source is now just that cut-out. Drag it, scale it, place it.
6. While streaming, if that UI is not in the sampled source anymore (loading, vehicle, hidden HUD, cutscene), this source **hides itself** until the UI is back.

One source instance = one highlighted element. Add three HUD Mask sources for meter + minimap + bars. Hide is per instance: the minimap can stay while the meter is gone.

There is **no internal database of games**. No NTE pack, no WoW pack, no growing list. Mechanisms only: highlight, clean, crop, sample, hide.

## Success (v1 = public / “it works”)

On **any** game, including ones we have never seen:

1. Add HUD Mask on **any** scene/canvas → pick **any** source → highlight an element → get a usable cut-out.
2. Place and scale it myself. The plugin does not auto-layout the scene. Intended layout is Aitum Vertical; technically it is just an OBS scene item.
3. When that UI disappears, the cut-out does not punch a hole of world/loading-screen through the overlay.
4. When the UI comes back, the cut-out comes back.
5. I do not need Photoshop, crop filters, or source clones for this job.
6. Dual-canvas streaming does not hitch.

NTE and WoW are **test cases**, not the product.

This is not a clone of Advanced Masks, Source Clone, or Advanced Scene Switcher. Those tools are how people approximate this today. See [prior-art.md](prior-art.md).

## Non-goals

- A catalog of games or shipped mask files.
- Asking the user to walk around so we can find UI by motion. Killed; it is a bad setup experience.
- Auto-placing cut-outs around the vertical canvas.
- Building the rest of the vertical scene (world crop, cam, alerts, chat, branding bar).
- Reading game memory or injecting into the game.
- Aitum-private APIs. Stream Suite is the intended canvas, not a dependency. This is a normal OBS source.
- A paid product. GPL-2.0-or-later, free to use, no telemetry.
- Pixel-perfect Photoshop replacement on the first highlight. Cleanup should be *good*; the user can re-highlight or erase if it missed.

## License and visibility

- Free to use under GPL-2.0-or-later (required to link libobs).
- Repo **private** until v1 is stable and actually usable.
- Then **public**, so other streamers can install it. We are not counting on outside contributors.

## Phase

**MVP** (this is the setup UI, not a later dock)

## Goal

The add-source / **Edit cutout…** window:

1. Live (or snapshot) view of the sampled source
2. **Highlighter** brush — user roughly paints over the element
3. Erase / reset
4. Plugin **cleans up** the stroke so the mask hugs the element
5. Crop = bounding box of the cleaned mask
6. Preview of the result
7. OK writes mask + crop onto the source

Rough highlight is the whole point. The user is not tracing in Photoshop and is not walking around the game.

## In scope

- Custom Qt dialog (stock `obs_properties` cannot host this)
- Brush + erase
- Cleanup inside a dilated box of the stroke (GrabCut / edge snap — spike one)
- **Keep gaps** so separate blobs become islands for auto-hide (#8)
- Feather
- Re-edit later

## Out of scope

- Auto-find every UI with no paint
- Motion calibration
- Perfect Photoshop outlines on the first stroke (erase and paint again)
- Auto-placing the item on the canvas

## Acceptance

- [ ] WoW Details meter: a rough highlight becomes a usable dark-panel cut-out
- [ ] WoW minimap / NTE radar: circular-ish hug, good enough
- [ ] WoW action bar: usable bar cut-out (mobile-quality, not pixel-perfect)
- [ ] User can reject, erase, and paint again
- [ ] Result works on main canvas and on Aitum Vertical

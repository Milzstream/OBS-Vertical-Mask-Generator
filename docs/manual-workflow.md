# Manual workflow this replaces

The usual way to put game HUD on a vertical canvas today:

1. Source-clone the game capture (or scene).
2. Crop the clone so it is just larger than the element.
3. Image Mask/Blend with a white-on-black PNG (often traced in Photoshop / Photopea).
4. Transform the clone on the vertical canvas.
5. Hide it by hand when the HUD is gone (menus, loading, vehicles, hide-UI).

Masks are authored at **crop size**, not full frame. White = keep, black = discard, edges usually feathered. Geometric HUD (circles, bars) is common; irregular clusters (ammo + weapon tiles) need a traced silhouette.

That stack works when the HUD is present and matches the PNG. It punches holes of world/background through the overlay when the HUD is missing, and it does not adapt when a kit shows fewer slots than the PNG was drawn for.

HUD Mask is meant to replace the clone + crop + PNG + manual hide chain. Existing PNG files are a visual reference for feathered cut-outs, not an import requirement.

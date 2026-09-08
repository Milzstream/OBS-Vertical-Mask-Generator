# Vision

## The job to be done

I stream horizontal and vertical at the same time with Aitum Stream Suite. The vertical canvas is a second OBS canvas. Gameplay is 16:9; the vertical frame is 9:16. HUD that lives in the corners of the horizontal image has to be extracted and re-composed onto the vertical scene so viewers can still see abilities, radar, health, and ammo.

## What I do today

For each HUD piece, in OBS:

1. Source-clone the raw game scene.
2. Crop the clone so it is just larger than the UI element.
3. Add an Image Mask/Blend filter using a Photoshop PNG from `Vertical UI Masks\<Type>\<Game>.png`.
4. Put that clone on the Aitum Vertical canvas and transform it into place.

Masks are white-on-black, feathered, authored at crop size (not full 2560×1440). Types already in use: Abilities, Radar, Health Bar, Level Bar, Weapons and Ammo. Games already in use: NTE, Stellar Blade, Destiny, Marathon, Alien Isolation.

A worked crop example (NTE radar, 2560×1440 source): left 45, top 10, right 2180, bottom 1065 → a 335×366 crop, then a circular mask.

This is documented with screenshots and files in [current-workflow.md](current-workflow.md).

## What goes wrong

Static masks do not know whether the HUD is actually there.

- **Menus / loading / fullscreen cinematics:** ability holes punch through to the world (or to a menu). The vertical overlay shows grass, a city, or a car instead of abilities.
- **Vehicles in NTE:** abilities hide, minimap stays. The ability mask still punches three circles of road into the overlay.
- **Character-specific HUD:** NTE ability count and layout are not universal. A 3-circle mask on a 4-slot character (or the reverse) leaves empty space or clips the wrong chrome.
- **Setup cost:** every new game is Photoshop + crop math + OBS filter stacking. Playing one game well is already a lot of work; swapping games is worse.

The hope is not "prettier PNGs". It is that the overlay can **track whether the HUD is present**, **hide or reshape when it is not**, and **take less manual work to set up**.

## What we are actually building

An OBS plugin that adds a **HUD Mask** source.

It is the vertical-canvas scene item for one HUD element. Internally it does the clone + crop + mask job, then adds a presence signal so the item can go fully transparent when that HUD is gone.

It is **not** a Photoshop replacement on day one. Existing PNGs must keep working. Generation and tracking come after the source is real and hide/show is trustworthy.

## Success criteria

The plugin is successful when, for a game I already mask by hand (NTE abilities + radar):

1. I can recreate the current look with one HUD Mask source per element, no source-clone + crop + image-mask stack.
2. On the character-select / menu screen, ability holes do **not** show the background. Radar may still show if the radar is visible.
3. In a vehicle, abilities hide and radar can stay.
4. I can still drag and scale the item on the Aitum Vertical canvas.
5. CPU/GPU cost is small enough that dual-canvas streaming does not hitch.

If (2) and (3) are unreliable, the plugin is not better than the current PNGs.

## Non-goals (explicit)

- Reading game memory, injecting into the game, or anything that would trip anti-cheat (Destiny, etc.).
- Official Aitum Stream Suite integration. There is no public API we should depend on. A normal OBS source on the vertical canvas is the integration.
- A full-frame "find all UI in any game" detector as the first version. Automation should be **user-guided**: mark a region or move the camera, then let the plugin snap edges and build the mask.
- A paid product. The plugin will be free to use (GPL-2.0-or-later, the license required to link libobs).
- Replacing the Instagram / branding overlay (MilzOGram / Milzstream). That is a separate dock/browser graphic. This plugin only supplies the game HUD pixels that sit in or around it.
- macOS / Linux as a v1 requirement.
- Auto-building a complete vertical scene (gameplay crop, webcam, alerts, chat). HUD extraction only.

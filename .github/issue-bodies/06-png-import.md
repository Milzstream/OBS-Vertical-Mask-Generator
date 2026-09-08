## Phase

MVP

## Problem

There is already a library of handmade masks:

`C:\Users\Milz\OneDrive\Streams\Vertical UI Masks`

Types: Abilities, Radar, Health Bar, Level Bar, Weapons and Ammo.
Games: NTE, Stellar Blade, Destiny, Marathon, Alien Isolation.

Those files must keep working. The plugin is not allowed to require re-authoring them.

## Goal

White-on-black, crop-sized, feathered PNGs behave like OBS Image Mask/Blend.

## In scope

- Document the mask convention in user-facing README
- Load PNG from an arbitrary path
- Alpha from the red/luma channel (match Image Mask/Blend “Alpha Mask Blend” / intensity)
- Do **not** commit personal PNGs/PSDs to git

## Out of scope

- PSD loading
- Auto-finding that OneDrive folder
- Converting full-frame masks

## Acceptance

- [ ] `Radar/NTE.png` on the documented 335×366 crop looks like today’s radar overlay
- [ ] `Abilities/NTE.png` matches today’s three-circle ability overlay
- [ ] A hard-edged test PNG still works
- [ ] A Destiny weapons mask (irregular shape) punches the right silhouette

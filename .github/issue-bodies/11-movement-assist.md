## Phase

v2 / research-backed feature

## Problem

Even drawing loose boxes is work. HUD is screen-space; the world moves. If we ask the user to look around for ~2 seconds we can keep pixels that did **not** move and propose HUD regions.

## Goal

A “Calibrate with movement” action:

1. Prompt: look around / walk / turn the camera for ~2 seconds
2. Accumulate a downscaled variance map of the sampled source
3. Low-variance connected components become candidate HUD slots
4. User taps which blobs are actually HUD (radar vs a static prop)
5. Each accepted blob is sent through the same edge-snap as the mark-based dock

## Why this might work

- NTE / Destiny radar and ability chrome stay put while the world streams by
- We only need candidate **boxes**, not a perfect mask, if edge-snap follows

## Why this will fail sometimes

- Menus: everything is static → the whole screen lights up
- Cutscenes / loading: no HUD, or HUD + letterbox both static
- Translucent HUD over a still sky
- User standing still

So this **proposes**, it does not auto-commit. Menus should be excluded from the capture window (on-screen instruction: “do this in gameplay”).

## In scope

- Variance pass on a downscaled frame
- Blob proposal + user confirm
- Handoff to mask snap

## Out of scope

- Unattended full-frame autodetect with no confirmation
- Using this as the presence signal (presence is template/probe; this is setup only)

## Acceptance

- [ ] Spike notes with screenshots: NTE gameplay look-around proposes radar (top-left) and abilities (bottom-right) as separate blobs
- [ ] Menu capture is documented as a failure mode, not silently accepted
- [ ] If the spike is too noisy, this issue is closed as wontfix without blocking v1

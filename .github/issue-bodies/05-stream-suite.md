## Phase

MVP

## Goal

Intended environment: OBS 32 + Aitum Stream Suite vertical canvas, sampling a game source that lives on the **main** canvas.

Also required: the same source works on a **vanilla** OBS scene (no Stream Suite). This is a general cut-out, not an Aitum plugin.

## In scope

- Manual test: HUD Mask on AITUM VERTICAL, target = main Game Capture
- Manual test: HUD Mask on MAIN CANVAS, target = another source in the same scene
- Transform, hide, copy, scene collection switch
- Notes on extra-canvas sampling quirks

## Out of scope

- Linking Stream Suite
- Scene-link automation

## Acceptance

- [ ] Vertical canvas: add, render, transform, no crash on dock toggle
- [ ] Main canvas: same
- [ ] Dual-canvas preview does not hitch vs today's clone stack

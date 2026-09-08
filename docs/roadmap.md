# Roadmap

Phased so each step is usable in a real Stream Suite scene. GitHub issues map to these phases.

## Phase 0 — this repo

- Product docs (`docs/`)
- Issue templates and feature issues
- Empty plugin layout (`src/`, `data/`, `profiles/`)
- No OBS binary

## Phase MVP — "one source instead of three filters"

**Goal:** On the Aitum Vertical canvas I can add a HUD Mask, point it at the game, set crop + PNG, and get the same look as clone + crop + Image Mask/Blend.

- Bootstrap from obs-plugintemplate (Windows, OBS 32)
- HUD Mask source: sample, crop, PNG mask, alpha output
- Properties: source picker, crop insets, mask file, feather
- Existing `Vertical UI Masks` PNGs work unchanged
- Manual hide still works (stock source visibility)
- Install and confirm it appears on the Stream Suite vertical canvas and can be transformed

**Exit:** NTE radar + NTE abilities look like they do today, with fewer OBS objects.

## Phase v1 — "don't show holes when the HUD is gone"

**Goal:** The NTE screenshots of menu and car no longer bleed world through the ability holes, without me toggling visibility by hand.

- Presence detection (probe, then template)
- Hysteresis + fade
- Per-instance (abilities hide, radar can stay)
- Capture-reference in properties
- Game profile JSON that can stamp multiple slots
- Test matrix: NTE menu, NTE car, NTE default combat, one other game (Destiny radar or Stellar Blade health)

**Exit:** I can play NTE on vertical without micromanaging ability visibility, and a false hide is rare enough to ignore.

## Phase v1.5 — "setup is not Photoshop + a calculator"

- Calibration dock: snapshot, **roughly mark / circle** HUD pieces, preview mask, presence score meter
- Edge / flood snap from the mark so the user does not trace pixel-perfect
- Import a folder of existing masks into a profile
- Optional: remember last crop per game

## Phase v2 — "help make the mask, adapt when the kit changes"

Experiments, each allowed to fail without blocking v1:

- **Request movement** (~2s look-around) and keep low-variance pixels as HUD candidates
- Assisted mask from a marked crop (threshold / flood / edges)
- NTE ability circle detection → generated mask + hide when count is 0
- Profile variants (3-slot vs 4-slot) with auto-pick by best template

## Explicitly later / maybe never

- Full-frame automatic HUD discovery
- macOS / Linux as a supported platform
- HDR-perfect sampling
- Filter form factor
- Any Aitum-private hooks

## Suggested first implementation issues

See GitHub. The intended first code issue is plugin bootstrap, then the HUD Mask source, then PNG parity, then presence on NTE abilities.

# Game profiles

JSON packs that describe HUD slots for a game: crop insets, mask files, presence settings.

Nothing ships here yet except the schema. Personal PNGs/PSDs stay in OneDrive (`Vertical UI Masks`) and are not committed.

First profiles to encode once the source exists:

| Game | Slots |
| --- | --- |
| NTE | Radar, Abilities, Level Bar |
| Destiny | Radar, Weapons and Ammo |
| Stellar Blade | Abilities, Health Bar |
| Marathon | Health Bar, Weapons and Ammo |
| Alien Isolation | Health Bar |

Crop values use OBS-style insets from each edge of the sampled source, matching `docs/current-workflow.md`.

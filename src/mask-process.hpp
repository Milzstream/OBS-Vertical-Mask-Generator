#pragma once

#include <cstdint>
#include <vector>

/* Pure mask helpers — no OBS/Qt. Expand/contract then optional edge feather. */

void mask_binarize(std::vector<uint8_t> &gray, uint8_t threshold = 128);

/* radius > 0 grows the opaque region; radius < 0 shrinks it. */
void mask_expand(std::vector<uint8_t> &gray, int width, int height, int radius);

/* Soften the edge over `radius` pixels after a binary mask. */
void mask_feather(std::vector<uint8_t> &gray, int width, int height, int radius);

bool mask_is_roundish(int bbox_w, int bbox_h, int filled_pixels);
int mask_circle_radius_from_bounds(int bbox_w, int bbox_h);

/* Flood-fill empty pixels from (x,y). Pixels >= 20 are walls. Returns false if
 * the seed is out of bounds or on a wall. 1px holes in a wall (painted on
 * opposite sides) are treated as closed so the fill does not leak. If
 * absorb_outline, walls that touch the filled region become filled so a traced
 * outline is not left as a ring. */
bool mask_flood_fill(std::vector<uint8_t> &gray, int width, int height, int x, int y,
		     bool absorb_outline = true);

/* Erase the connected painted blob containing (x,y). Pixels >= 20 are painted.
 * 1px gaps between painted pixels are crossed. Returns false if the seed is
 * out of bounds or not painted. */
bool mask_flood_erase(std::vector<uint8_t> &gray, int width, int height, int x, int y);

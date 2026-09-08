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

#pragma once

#include <cstdint>
#include <vector>

/* Pure mask helpers — no OBS/Qt. */

struct MaskPoint {
	float x = 0;
	float y = 0;
};

struct MaskCrop {
	int min_x = 0;
	int min_y = 0;
	int max_x = -1;
	int max_y = -1;
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	bool empty = true;
};

void mask_binarize(std::vector<uint8_t> &gray, uint8_t threshold = 128);

/* radius > 0 grows the opaque region; radius < 0 shrinks it. */
void mask_expand(std::vector<uint8_t> &gray, int width, int height, int radius);

/* Soften the edge over `radius` pixels after a binary mask. */
void mask_feather(std::vector<uint8_t> &gray, int width, int height, int radius);

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

/* Sobel magnitude of a luma plane. Returns the max magnitude (at least 1). */
int mask_sobel(const std::vector<uint8_t> &lum, int width, int height, std::vector<uint16_t> &mag);

/* Opaque bbox of pixels >= threshold, then pad. Insets are relative to the
 * full frame (right = width-1-max_x). empty if nothing is opaque. */
MaskCrop mask_crop_from_opaque(const std::vector<uint8_t> &gray, int width, int height, uint8_t threshold = 20,
			       int pad = 32);

/* Snap painted pixels (>= 40) onto nearby luma edges. Writes 0/255 (+ a 3px
 * inward feather) into `out`. Returns false if there is not enough paint. */
bool mask_snap_edges(const std::vector<uint8_t> &user, const std::vector<uint8_t> &lum, int width, int height,
		     int search, std::vector<uint8_t> &out);

/* Pull a user loop onto the nearest outer luma edge. `out` is the densified
 * shrinkwrapped polygon. Returns false if the loop is too short. */
bool mask_magic_shrinkwrap(const std::vector<MaskPoint> &loop, const std::vector<uint8_t> &lum, int width, int height,
			   int search, std::vector<MaskPoint> &out);

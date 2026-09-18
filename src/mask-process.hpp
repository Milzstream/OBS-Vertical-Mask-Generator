#pragma once

#include <cstdint>
#include <string>
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

/* Snap painted pixels (>= 40) onto nearby luma edges. Writes a hard 0/255
 * fill. Prefers the nearest strong edge to the paint boundary so inner art
 * does not pull the mask in. Returns false if there is not enough paint. */
bool mask_snap_edges(const std::vector<uint8_t> &user, const std::vector<uint8_t> &lum, int width, int height,
		     int search, std::vector<uint8_t> &out);

/* Pull a user loop onto the nearest outer luma edge. `out` is the densified
 * shrinkwrapped polygon. Returns false if the loop is too short. */
bool mask_magic_shrinkwrap(const std::vector<MaskPoint> &loop, const std::vector<uint8_t> &lum, int width, int height,
			   int search, std::vector<MaskPoint> &out);

/* Interior of the paint, inset from empty so a slightly loose mask does not
 * pick up the world. If luma is set, prefer pixels that had edges in the still
 * (HUD structure, not flat glass). Falls back to the whole inset region. */
void mask_presence_band(const std::vector<uint8_t> &mask, const std::vector<uint8_t> *luma, int width, int height,
			int inset, std::vector<uint8_t> &band);

void mask_resize_luma(const std::vector<uint8_t> &src, int src_w, int src_h, std::vector<uint8_t> &dst, int dst_w,
		      int dst_h);

void mask_resize_mask(const std::vector<uint8_t> &src, int src_w, int src_h, std::vector<uint8_t> &dst, int dst_w,
		      int dst_h);

/* Binarize + expand + feather, same order as the visible mask texture. */
void mask_presence_prepare_mask(std::vector<uint8_t> &gray, int width, int height, int expand, int feather);

/* Pearson correlation of edge strength on band pixels (shape, not color). */
bool mask_presence_score(const std::vector<uint8_t> &ref_luma, const std::vector<uint8_t> &cur_luma,
			 const std::vector<uint8_t> &band, int width, int height, float *score);

/* Best Pearson over a small translation search and a few smaller scales
 * (HUD drift + empty slots). max_shift is clamped to 0–8. */
bool mask_presence_match(const std::vector<uint8_t> &ref_luma, const std::vector<uint8_t> &cur_luma,
			 const std::vector<uint8_t> &band, int width, int height, int max_shift, float *score);

/* Next name after last in a sorted unique list. Empty last or unknown last wraps to front. */
std::string mask_presence_next_target(const std::vector<std::string> &sorted_unique, const std::string &last);

/* 1px outline of the painted blob (mask >= 20). */
void mask_blob_outline(const std::vector<uint8_t> &mask, int width, int height, std::vector<uint8_t> &outline);

/* Fraction of the painted outline that still sits on a live edge. Tries a few
 * smaller scales so an empty slot (same shape, smaller) still scores. If
 * prev_luma is set, edges that moved with the world are down-weighted. */
bool mask_presence_outline_score(const std::vector<uint8_t> &mask, const std::vector<uint8_t> &cur_luma,
				 const std::vector<uint8_t> *prev_luma, int width, int height, float *score);

/* Map a 0–100 match slider to a hide threshold. 0 is lenient, 100 is strict. */
float mask_presence_threshold(int match_percent);

struct MaskPresenceGate {
	bool shown = true;
	int streak = 0;
};

/* Hide/show with a deadband around the match line and a streak before flipping.
 * Returns true when shown changed. hysteresis < 1 is treated as 1. */
bool mask_presence_gate(MaskPresenceGate &gate, float score, int match_percent, int hysteresis = 3);

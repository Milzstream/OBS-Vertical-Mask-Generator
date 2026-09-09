#pragma once

#include <obs-module.h>

#include <string>
#include <vector>

struct hud_mask {
	obs_source_t *self = nullptr;
	obs_weak_source_t *target = nullptr;
	gs_texrender_t *texrender = nullptr;
	gs_texture_t *mask_tex = nullptr;
	bool mask_loaded = false;
	bool rendering = false;

	std::string target_name;
	std::string canvas_uuid;
	std::string mask_path;

	int crop_left = 0;
	int crop_top = 0;
	int crop_right = 0;
	int crop_bottom = 0;
	int expand = 0;
	int feather = 0;
	bool auto_hide = false;
	int fade_ms = 200;
	int match_pct = 50;

	std::vector<uint8_t> mask_gray;
	int mask_w = 0;
	int mask_h = 0;

	bool ref_valid = false;
	bool ref_capture_pending = false;
	int ref_w = 0;
	int ref_h = 0;
	std::vector<uint8_t> ref_luma;
	std::vector<uint8_t> ref_band;

	bool presence_shown = true;
	int presence_streak = 0;
	float draw_alpha = 1.0f;
	std::vector<uint8_t> presence_prev;
	int presence_prev_w = 0;
	int presence_prev_h = 0;

	uint32_t src_cx = 0;
	uint32_t src_cy = 0;
	uint32_t cx = 0;
	uint32_t cy = 0;
};

void register_hud_mask_source(void);
void hud_mask_load_effects(void);
void hud_mask_unload_effects(void);

obs_source_t *hud_mask_get_target(hud_mask *ctx);
void hud_mask_set_cutout(hud_mask *ctx, const char *path, int left, int top, int right, int bottom);

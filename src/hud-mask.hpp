#pragma once

#include <obs-module.h>

#include <string>

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

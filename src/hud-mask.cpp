/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "hud-mask.hpp"

#include <obs-module.h>
#include <graphics/image-file.h>
#include <plugin-support.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char *k_id = "vertical_hud_mask";
constexpr const char *k_kind = "target_kind";
constexpr const char *k_source = "target";
constexpr const char *k_crop_left = "crop_left";
constexpr const char *k_crop_top = "crop_top";
constexpr const char *k_crop_right = "crop_right";
constexpr const char *k_crop_bottom = "crop_bottom";
constexpr const char *k_mask_path = "mask_path";
constexpr const char *k_auto_hide = "auto_hide";

gs_effect_t *mask_effect = nullptr;

struct hud_mask {
	obs_source_t *self = nullptr;
	obs_weak_source_t *target = nullptr;
	gs_texrender_t *texrender = nullptr;
	gs_image_file_t mask_image{};
	bool mask_loaded = false;
	bool rendering = false;

	std::string target_name;
	std::string mask_path;

	int crop_left = 0;
	int crop_top = 0;
	int crop_right = 0;
	int crop_bottom = 0;
	bool auto_hide = false;

	uint32_t src_cx = 0;
	uint32_t src_cy = 0;
	uint32_t cx = 0;
	uint32_t cy = 0;
};

obs_source_t *acquire_target(hud_mask *ctx)
{
	if (ctx->target)
		return obs_weak_source_get_source(ctx->target);

	if (ctx->target_name.empty())
		return nullptr;

	obs_source_t *source = obs_get_source_by_name(ctx->target_name.c_str());
	if (!source)
		return nullptr;

	if (source == ctx->self) {
		obs_source_release(source);
		return nullptr;
	}

	return source;
}

void release_target(hud_mask *ctx, obs_source_t *source)
{
	if (!source)
		return;
	if (ctx->target)
		obs_source_remove_active_child(ctx->self, source);
	obs_source_release(source);
}

void set_target(hud_mask *ctx, const char *name)
{
	obs_source_t *prev = nullptr;
	if (ctx->target) {
		prev = obs_weak_source_get_source(ctx->target);
		if (prev)
			obs_source_remove_active_child(ctx->self, prev);
		obs_weak_source_release(ctx->target);
		ctx->target = nullptr;
	}

	ctx->target_name = name ? name : "";

	if (ctx->target_name.empty()) {
		if (prev)
			obs_source_release(prev);
		return;
	}

	obs_source_t *next = obs_get_source_by_name(ctx->target_name.c_str());
	if (!next || next == ctx->self) {
		if (next)
			obs_source_release(next);
		if (prev)
			obs_source_release(prev);
		return;
	}

	if (!obs_source_add_active_child(ctx->self, next)) {
		obs_log(LOG_WARNING, "refusing target '%s' (cycle)", ctx->target_name.c_str());
		obs_source_release(next);
		if (prev)
			obs_source_release(prev);
		ctx->target_name.clear();
		return;
	}

	ctx->target = obs_source_get_weak_source(next);
	obs_source_release(next);
	if (prev)
		obs_source_release(prev);
}

void free_mask_texture(hud_mask *ctx)
{
	if (!ctx->mask_loaded)
		return;
	obs_enter_graphics();
	gs_image_file_free(&ctx->mask_image);
	obs_leave_graphics();
	ctx->mask_loaded = false;
	memset(&ctx->mask_image, 0, sizeof(ctx->mask_image));
}

void load_mask_texture(hud_mask *ctx, const char *path)
{
	free_mask_texture(ctx);
	ctx->mask_path = path ? path : "";
	if (ctx->mask_path.empty())
		return;

	gs_image_file_init(&ctx->mask_image, ctx->mask_path.c_str());
	obs_enter_graphics();
	gs_image_file_init_texture(&ctx->mask_image);
	obs_leave_graphics();

	if (!ctx->mask_image.loaded || !ctx->mask_image.texture) {
		obs_log(LOG_WARNING, "failed to load mask '%s'", ctx->mask_path.c_str());
		gs_image_file_free(&ctx->mask_image);
		memset(&ctx->mask_image, 0, sizeof(ctx->mask_image));
		return;
	}

	ctx->mask_loaded = true;
}

void update_size(hud_mask *ctx)
{
	obs_source_t *target = acquire_target(ctx);
	if (!target) {
		ctx->src_cx = ctx->src_cy = ctx->cx = ctx->cy = 0;
		return;
	}

	ctx->src_cx = obs_source_get_base_width(target);
	ctx->src_cy = obs_source_get_base_height(target);
	obs_source_release(target);

	const int left = std::max(ctx->crop_left, 0);
	const int top = std::max(ctx->crop_top, 0);
	const int right = std::max(ctx->crop_right, 0);
	const int bottom = std::max(ctx->crop_bottom, 0);

	const int w = static_cast<int>(ctx->src_cx) - left - right;
	const int h = static_cast<int>(ctx->src_cy) - top - bottom;
	ctx->cx = w > 0 ? static_cast<uint32_t>(w) : 0;
	ctx->cy = h > 0 ? static_cast<uint32_t>(h) : 0;
}

const char *hud_mask_get_name(void *)
{
	return obs_module_text("HUDMask");
}

void *hud_mask_create(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new hud_mask();
	ctx->self = source;
	obs_source_update(source, settings);
	return ctx;
}

void hud_mask_destroy(void *data)
{
	auto *ctx = static_cast<hud_mask *>(data);
	set_target(ctx, "");
	free_mask_texture(ctx);
	if (ctx->texrender) {
		obs_enter_graphics();
		gs_texrender_destroy(ctx->texrender);
		obs_leave_graphics();
	}
	delete ctx;
}

void hud_mask_update(void *data, obs_data_t *settings)
{
	auto *ctx = static_cast<hud_mask *>(data);
	set_target(ctx, obs_data_get_string(settings, k_source));
	ctx->crop_left = static_cast<int>(obs_data_get_int(settings, k_crop_left));
	ctx->crop_top = static_cast<int>(obs_data_get_int(settings, k_crop_top));
	ctx->crop_right = static_cast<int>(obs_data_get_int(settings, k_crop_right));
	ctx->crop_bottom = static_cast<int>(obs_data_get_int(settings, k_crop_bottom));
	ctx->auto_hide = obs_data_get_bool(settings, k_auto_hide);
	load_mask_texture(ctx, obs_data_get_string(settings, k_mask_path));
	update_size(ctx);
}

void hud_mask_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, k_kind, "source");
	obs_data_set_default_string(settings, k_source, "");
	obs_data_set_default_int(settings, k_crop_left, 0);
	obs_data_set_default_int(settings, k_crop_top, 0);
	obs_data_set_default_int(settings, k_crop_right, 0);
	obs_data_set_default_int(settings, k_crop_bottom, 0);
	obs_data_set_default_string(settings, k_mask_path, "");
	obs_data_set_default_bool(settings, k_auto_hide, false);
}

struct collect_ctx {
	std::vector<std::string> *names = nullptr;
	bool scenes = false;
	obs_source_t *self = nullptr;
};

bool collect_targets(void *param, obs_source_t *source)
{
	auto *e = static_cast<collect_ctx *>(param);
	if (!source || source == e->self)
		return true;

	const bool is_scene = obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE;
	if (e->scenes != is_scene)
		return true;

	if (!e->scenes) {
		if (obs_source_get_type(source) != OBS_SOURCE_TYPE_INPUT)
			return true;
		if ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) == 0)
			return true;
		const char *id = obs_source_get_id(source);
		if (id && strcmp(id, k_id) == 0)
			return true;
	}

	const char *name = obs_source_get_name(source);
	if (!name || !name[0])
		return true;

	e->names->emplace_back(name);
	return true;
}

void fill_target_list(obs_property_t *list, const char *kind, obs_source_t *self)
{
	obs_property_list_clear(list);
	obs_property_list_add_string(list, obs_module_text("HUDMask.Source.None"), "");

	const bool scenes = kind && strcmp(kind, "scene") == 0;
	std::vector<std::string> names;
	collect_ctx e{&names, scenes, self};
	if (scenes)
		obs_enum_scenes(collect_targets, &e);
	else
		obs_enum_sources(collect_targets, &e);

	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	for (const auto &name : names)
		obs_property_list_add_string(list, name.c_str(), name.c_str());

	obs_property_set_description(list, obs_module_text(scenes ? "HUDMask.Scene" : "HUDMask.Source"));
}

bool kind_modified(void *priv, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto *ctx = static_cast<hud_mask *>(priv);
	fill_target_list(obs_properties_get(props, k_source), obs_data_get_string(settings, k_kind),
			 ctx ? ctx->self : nullptr);
	return true;
}

obs_properties_t *hud_mask_properties(void *data)
{
	auto *ctx = static_cast<hud_mask *>(data);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *kind = obs_properties_add_list(props, k_kind, obs_module_text("HUDMask.Type"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(kind, obs_module_text("HUDMask.Type.Source"), "source");
	obs_property_list_add_string(kind, obs_module_text("HUDMask.Type.Scene"), "scene");
	obs_property_set_modified_callback2(kind, kind_modified, ctx);

	obs_property_t *targets = obs_properties_add_list(props, k_source, obs_module_text("HUDMask.Source"),
							  OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	fill_target_list(targets, "source", ctx ? ctx->self : nullptr);

	obs_properties_add_int(props, k_crop_left, obs_module_text("HUDMask.Crop.Left"), 0, 8192, 1);
	obs_properties_add_int(props, k_crop_top, obs_module_text("HUDMask.Crop.Top"), 0, 8192, 1);
	obs_properties_add_int(props, k_crop_right, obs_module_text("HUDMask.Crop.Right"), 0, 8192, 1);
	obs_properties_add_int(props, k_crop_bottom, obs_module_text("HUDMask.Crop.Bottom"), 0, 8192, 1);

	obs_properties_add_path(props, k_mask_path, obs_module_text("HUDMask.MaskImage"), OBS_PATH_FILE,
				obs_module_text("HUDMask.MaskImage.Filter"), nullptr);

	obs_property_t *auto_hide =
		obs_properties_add_bool(props, k_auto_hide, obs_module_text("HUDMask.AutoHide.Enable"));
	obs_property_set_long_description(auto_hide, obs_module_text("HUDMask.AutoHide.Unavailable"));
	obs_property_set_enabled(auto_hide, false);

	return props;
}

uint32_t hud_mask_width(void *data)
{
	return static_cast<hud_mask *>(data)->cx;
}

uint32_t hud_mask_height(void *data)
{
	return static_cast<hud_mask *>(data)->cy;
}

void hud_mask_tick(void *data, float)
{
	update_size(static_cast<hud_mask *>(data));
}

void draw_texture(gs_texture_t *tex, uint32_t cx, uint32_t cy, gs_texture_t *mask)
{
	if (!tex)
		return;

	gs_effect_t *effect = nullptr;
	gs_eparam_t *image_param = nullptr;

	if (mask && mask_effect) {
		effect = mask_effect;
		image_param = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image_param, tex);
		gs_eparam_t *mask_param = gs_effect_get_param_by_name(effect, "mask");
		gs_effect_set_texture(mask_param, mask);
	} else {
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		image_param = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image_param, tex);
	}

	while (gs_effect_loop(effect, "Draw"))
		gs_draw_sprite(tex, 0, cx, cy);
}

void hud_mask_render(void *data, gs_effect_t *)
{
	auto *ctx = static_cast<hud_mask *>(data);
	if (ctx->rendering || ctx->cx == 0 || ctx->cy == 0)
		return;

	obs_source_t *target = acquire_target(ctx);
	if (!target)
		return;

	ctx->rendering = true;

	if (!ctx->texrender)
		ctx->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	else
		gs_texrender_reset(ctx->texrender);

	const float left = static_cast<float>(std::max(ctx->crop_left, 0));
	const float top = static_cast<float>(std::max(ctx->crop_top, 0));

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(ctx->texrender, ctx->cx, ctx->cy)) {
		struct vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(left, left + static_cast<float>(ctx->cx), top, top + static_cast<float>(ctx->cy), -100.0f,
			 100.0f);
		obs_source_video_render(target);
		gs_texrender_end(ctx->texrender);
	}

	gs_blend_state_pop();

	gs_texture_t *tex = gs_texrender_get_texture(ctx->texrender);
	gs_texture_t *mask = ctx->mask_loaded ? ctx->mask_image.texture : nullptr;
	draw_texture(tex, ctx->cx, ctx->cy, mask);

	ctx->rendering = false;
	obs_source_release(target);
}

obs_source_info make_info()
{
	obs_source_info info = {};
	info.id = k_id;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB;
	info.get_name = hud_mask_get_name;
	info.create = hud_mask_create;
	info.destroy = hud_mask_destroy;
	info.update = hud_mask_update;
	info.get_defaults = hud_mask_defaults;
	info.get_properties = hud_mask_properties;
	info.get_width = hud_mask_width;
	info.get_height = hud_mask_height;
	info.video_tick = hud_mask_tick;
	info.video_render = hud_mask_render;
	info.icon_type = OBS_ICON_TYPE_COLOR;
	return info;
}

} // namespace

void hud_mask_load_effects(void)
{
	char *path = obs_module_file("effects/hud-mask.effect");
	if (!path)
		return;
	obs_enter_graphics();
	mask_effect = gs_effect_create_from_file(path, nullptr);
	obs_leave_graphics();
	if (!mask_effect)
		obs_log(LOG_WARNING, "failed to load hud-mask.effect");
	bfree(path);
}

void hud_mask_unload_effects(void)
{
	obs_enter_graphics();
	if (mask_effect) {
		gs_effect_destroy(mask_effect);
		mask_effect = nullptr;
	}
	obs_leave_graphics();
}

void register_hud_mask_source(void)
{
	static obs_source_info info = make_info();
	obs_register_source(&info);
}

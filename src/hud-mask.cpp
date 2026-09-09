/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "hud-mask.hpp"
#include "cutout-editor.hpp"
#include "mask-process.hpp"
#include "presence.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>
#include <plugin-support.h>

#include <QImage>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char *k_id = "vertical_hud_mask";
constexpr const char *k_kind = "target_kind";
constexpr const char *k_canvas = "target_canvas";
constexpr const char *k_source = "target";
constexpr const char *k_same = "same_masks";
constexpr int k_same_cap = 8;
constexpr uint32_t k_canvas_flag_main = 1u << 0;
constexpr uint32_t k_canvas_flag_ephemeral = 1u << 4;
constexpr const char *k_crop_left = "crop_left";
constexpr const char *k_crop_top = "crop_top";
constexpr const char *k_crop_right = "crop_right";
constexpr const char *k_crop_bottom = "crop_bottom";
constexpr const char *k_mask_path = "mask_path";
constexpr const char *k_expand = "expand";
constexpr const char *k_feather = "feather";
constexpr const char *k_auto_hide = "auto_hide";
constexpr const char *k_fade = "auto_hide_fade";
constexpr const char *k_match = "auto_hide_match";

gs_effect_t *mask_effect = nullptr;

obs_canvas_t *find_canvas_by_uuid(const char *uuid)
{
	struct find_canvas_ctx {
		const char *uuid = nullptr;
		obs_canvas_t *found = nullptr;
	} e{uuid, nullptr};

	auto cb = [](void *param, obs_canvas_t *canvas) -> bool {
		auto *f = static_cast<find_canvas_ctx *>(param);
		if (!canvas)
			return true;
		if (obs_canvas_get_flags(canvas) & k_canvas_flag_ephemeral)
			return true;
		const char *id = obs_canvas_get_uuid(canvas);
		if (f->uuid && f->uuid[0]) {
			if (id && strcmp(id, f->uuid) == 0) {
				f->found = obs_canvas_get_ref(canvas);
				return false;
			}
			return true;
		}
		if (obs_canvas_get_flags(canvas) & k_canvas_flag_main) {
			f->found = obs_canvas_get_ref(canvas);
			return false;
		}
		return true;
	};
	obs_enum_canvases(cb, &e);
	if (e.found)
		return e.found;
	if (uuid && uuid[0])
		return nullptr;
	return obs_get_main_canvas();
}

obs_source_t *acquire_target(hud_mask *ctx)
{
	if (ctx->target)
		return obs_weak_source_get_source(ctx->target);

	if (ctx->target_name.empty())
		return nullptr;

	obs_source_t *source = nullptr;
	if (!ctx->canvas_uuid.empty()) {
		obs_canvas_t *canvas = find_canvas_by_uuid(ctx->canvas_uuid.c_str());
		if (canvas) {
			obs_scene_t *scene = obs_canvas_get_scene_by_name(canvas, ctx->target_name.c_str());
			obs_canvas_release(canvas);
			if (scene) {
				source = obs_source_get_ref(obs_scene_get_source(scene));
				obs_scene_release(scene);
			}
		}
	}
	if (!source)
		source = obs_get_source_by_name(ctx->target_name.c_str());
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
	if (!ctx->mask_tex && !ctx->mask_loaded)
		return;
	obs_enter_graphics();
	if (ctx->mask_tex) {
		gs_texture_destroy(ctx->mask_tex);
		ctx->mask_tex = nullptr;
	}
	obs_leave_graphics();
	ctx->mask_loaded = false;
}

void load_mask_texture(hud_mask *ctx, const char *path)
{
	free_mask_texture(ctx);
	ctx->mask_path = path ? path : "";
	ctx->mask_gray.clear();
	ctx->mask_w = ctx->mask_h = 0;
	if (ctx->mask_path.empty()) {
		hud_mask_presence_clear_ref(ctx);
		return;
	}

	QImage img(QString::fromUtf8(ctx->mask_path.c_str()));
	if (img.isNull()) {
		obs_log(LOG_WARNING, "failed to load mask '%s'", ctx->mask_path.c_str());
		return;
	}
	img = img.convertToFormat(QImage::Format_Grayscale8);
	const int w = img.width();
	const int h = img.height();
	if (w < 1 || h < 1)
		return;

	std::vector<uint8_t> gray(static_cast<size_t>(w) * h);
	for (int y = 0; y < h; y++)
		memcpy(gray.data() + static_cast<size_t>(y) * w, img.constScanLine(y), static_cast<size_t>(w));

	ctx->mask_gray = gray;
	ctx->mask_w = w;
	ctx->mask_h = h;
	hud_mask_presence_load_ref(ctx);
	if (ctx->auto_hide && !ctx->ref_valid)
		ctx->ref_capture_pending = true;

	if (ctx->expand != 0 || ctx->feather > 0) {
		mask_binarize(gray);
		if (ctx->expand != 0)
			mask_expand(gray, w, h, ctx->expand);
		if (ctx->feather > 0)
			mask_feather(gray, w, h, ctx->feather);
	}

	const uint8_t *slices[1] = {gray.data()};
	obs_enter_graphics();
	ctx->mask_tex = gs_texture_create(static_cast<uint32_t>(w), static_cast<uint32_t>(h), GS_R8, 1, slices, 0);
	obs_leave_graphics();
	ctx->mask_loaded = ctx->mask_tex != nullptr;
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
	hud_mask_presence_register(ctx);
	obs_source_update(source, settings);
	return ctx;
}

void hud_mask_destroy(void *data)
{
	auto *ctx = static_cast<hud_mask *>(data);
	hud_mask_presence_unregister(ctx);
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
	const char *canvas = obs_data_get_string(settings, k_canvas);
	ctx->canvas_uuid = canvas ? canvas : "";
	set_target(ctx, obs_data_get_string(settings, k_source));
	ctx->crop_left = static_cast<int>(obs_data_get_int(settings, k_crop_left));
	ctx->crop_top = static_cast<int>(obs_data_get_int(settings, k_crop_top));
	ctx->crop_right = static_cast<int>(obs_data_get_int(settings, k_crop_right));
	ctx->crop_bottom = static_cast<int>(obs_data_get_int(settings, k_crop_bottom));
	ctx->expand = static_cast<int>(obs_data_get_int(settings, k_expand));
	ctx->feather = static_cast<int>(obs_data_get_int(settings, k_feather));
	ctx->auto_hide = obs_data_get_bool(settings, k_auto_hide);
	ctx->fade_ms = static_cast<int>(obs_data_get_int(settings, k_fade));
	ctx->match_pct = static_cast<int>(obs_data_get_int(settings, k_match));
	if (ctx->fade_ms < 0)
		ctx->fade_ms = 0;
	if (ctx->fade_ms > 2000)
		ctx->fade_ms = 2000;
	if (ctx->match_pct < 0)
		ctx->match_pct = 0;
	if (ctx->match_pct > 100)
		ctx->match_pct = 100;
	load_mask_texture(ctx, obs_data_get_string(settings, k_mask_path));
	if (!ctx->auto_hide) {
		ctx->presence_shown = true;
		ctx->draw_alpha = 1.0f;
		ctx->presence_streak = 0;
		ctx->ref_capture_pending = false;
	} else if (!ctx->ref_valid) {
		ctx->ref_capture_pending = true;
	}
	update_size(ctx);
}

void hud_mask_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, k_kind, "source");
	{
		obs_canvas_t *main = obs_get_main_canvas();
		const char *uuid = main ? obs_canvas_get_uuid(main) : "";
		obs_data_set_default_string(settings, k_canvas, uuid ? uuid : "");
		if (main)
			obs_canvas_release(main);
	}
	obs_data_set_default_string(settings, k_source, "");
	obs_data_set_default_int(settings, k_crop_left, 0);
	obs_data_set_default_int(settings, k_crop_top, 0);
	obs_data_set_default_int(settings, k_crop_right, 0);
	obs_data_set_default_int(settings, k_crop_bottom, 0);
	obs_data_set_default_string(settings, k_mask_path, "");
	obs_data_set_default_int(settings, k_expand, 0);
	obs_data_set_default_int(settings, k_feather, 0);
	obs_data_set_default_bool(settings, k_auto_hide, false);
	obs_data_set_default_int(settings, k_fade, 200);
	obs_data_set_default_int(settings, k_match, 50);
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

void fill_canvas_list(obs_property_t *list)
{
	obs_property_list_clear(list);

	struct canvas_item {
		std::string name;
		std::string uuid;
		bool main = false;
	};
	std::vector<canvas_item> items;

	auto cb = [](void *param, obs_canvas_t *canvas) -> bool {
		auto *out = static_cast<std::vector<canvas_item> *>(param);
		if (!canvas)
			return true;
		const uint32_t flags = obs_canvas_get_flags(canvas);
		if (flags & k_canvas_flag_ephemeral)
			return true;
		const char *uuid = obs_canvas_get_uuid(canvas);
		if (!uuid || !uuid[0])
			return true;
		const char *name = obs_canvas_get_name(canvas);
		canvas_item item;
		item.uuid = uuid;
		item.main = (flags & k_canvas_flag_main) != 0;
		item.name = (name && name[0]) ? name
					     : (item.main ? obs_module_text("HUDMask.Canvas.Main") : uuid);
		out->push_back(std::move(item));
		return true;
	};
	obs_enum_canvases(cb, &items);

	std::sort(items.begin(), items.end(), [](const canvas_item &a, const canvas_item &b) {
		if (a.main != b.main)
			return a.main;
		return a.name < b.name;
	});
	if (items.empty())
		obs_property_list_add_string(list, obs_module_text("HUDMask.Canvas.Main"), "");
	for (const auto &item : items)
		obs_property_list_add_string(list, item.name.c_str(), item.uuid.c_str());
}

void fill_target_list(obs_property_t *list, const char *kind, const char *canvas_uuid, obs_source_t *self)
{
	obs_property_list_clear(list);
	obs_property_list_add_string(list, obs_module_text("HUDMask.Source.None"), "");

	const bool scenes = kind && strcmp(kind, "scene") == 0;
	std::vector<std::string> names;
	collect_ctx e{&names, scenes, self};
	if (scenes) {
		obs_canvas_t *canvas = find_canvas_by_uuid(canvas_uuid);
		if (canvas) {
			obs_canvas_enum_scenes(canvas, collect_targets, &e);
			obs_canvas_release(canvas);
		} else {
			obs_enum_scenes(collect_targets, &e);
		}
	} else {
		obs_enum_sources(collect_targets, &e);
	}

	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	for (const auto &name : names)
		obs_property_list_add_string(list, name.c_str(), name.c_str());

	obs_property_set_description(list, obs_module_text(scenes ? "HUDMask.Scene" : "HUDMask.Source"));
}

struct same_mask_ctx {
	obs_source_t *self = nullptr;
	const char *target = nullptr;
	std::vector<std::string> *names = nullptr;
};

bool collect_same_masks(void *param, obs_source_t *source)
{
	auto *e = static_cast<same_mask_ctx *>(param);
	if (!source || source == e->self)
		return true;
	const char *id = obs_source_get_unversioned_id(source);
	if (!id || strcmp(id, k_id) != 0)
		return true;
	obs_data_t *s = obs_source_get_settings(source);
	if (!s)
		return true;
	const char *t = obs_data_get_string(s, k_source);
	if (e->target && t && strcmp(e->target, t) == 0) {
		const char *n = obs_source_get_name(source);
		if (n && n[0])
			e->names->emplace_back(n);
	}
	obs_data_release(s);
	return true;
}

void fill_same_masks(obs_properties_t *props, obs_source_t *self, obs_data_t *settings)
{
	obs_property_t *prop = obs_properties_get(props, k_same);
	if (!prop || !settings)
		return;
	const char *target = obs_data_get_string(settings, k_source);
	std::vector<std::string> names;
	if (target && target[0]) {
		same_mask_ctx e{self, target, &names};
		obs_enum_sources(collect_same_masks, &e);
	}
	std::sort(names.begin(), names.end());
	if (names.empty()) {
		obs_data_unset_user_value(settings, k_same);
		obs_property_set_visible(prop, false);
		return;
	}
	std::string text;
	for (size_t i = 0; i < names.size() && i < static_cast<size_t>(k_same_cap); i++) {
		if (i)
			text += "\n";
		text += names[i];
	}
	if (names.size() > static_cast<size_t>(k_same_cap))
		text += "\n...";
	obs_data_set_string(settings, k_same, text.c_str());
	obs_property_set_visible(prop, true);
}

bool kind_modified(void *priv, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto *ctx = static_cast<hud_mask *>(priv);
	const char *kind = obs_data_get_string(settings, k_kind);
	const bool scenes = kind && strcmp(kind, "scene") == 0;
	obs_property_t *canvas = obs_properties_get(props, k_canvas);
	obs_property_set_visible(canvas, scenes);
	if (scenes)
		fill_canvas_list(canvas);
	fill_target_list(obs_properties_get(props, k_source), kind, obs_data_get_string(settings, k_canvas),
			 ctx ? ctx->self : nullptr);
	fill_same_masks(props, ctx ? ctx->self : nullptr, settings);
	return true;
}

bool canvas_modified(void *priv, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto *ctx = static_cast<hud_mask *>(priv);
	fill_target_list(obs_properties_get(props, k_source), obs_data_get_string(settings, k_kind),
			 obs_data_get_string(settings, k_canvas), ctx ? ctx->self : nullptr);
	fill_same_masks(props, ctx ? ctx->self : nullptr, settings);
	return true;
}

bool source_modified(void *priv, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto *ctx = static_cast<hud_mask *>(priv);
	fill_same_masks(props, ctx ? ctx->self : nullptr, settings);
	return true;
}

bool draw_mask_clicked(obs_properties_t *, obs_property_t *, void *priv)
{
	hud_mask_open_editor(static_cast<hud_mask *>(priv));
	return true;
}

bool auto_hide_modified(void *, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool has_mask = settings && obs_data_get_string(settings, k_mask_path) &&
			      obs_data_get_string(settings, k_mask_path)[0];
	const bool on = settings && obs_data_get_bool(settings, k_auto_hide);
	obs_property_t *hide = obs_properties_get(props, k_auto_hide);
	obs_property_t *fade = obs_properties_get(props, k_fade);
	obs_property_t *match = obs_properties_get(props, k_match);
	if (hide)
		obs_property_set_enabled(hide, has_mask);
	if (fade)
		obs_property_set_enabled(fade, has_mask && on);
	if (match)
		obs_property_set_enabled(match, has_mask && on);
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

	obs_property_t *canvas = obs_properties_add_list(props, k_canvas, obs_module_text("HUDMask.Canvas"),
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	fill_canvas_list(canvas);
	obs_property_set_modified_callback2(canvas, canvas_modified, ctx);

	obs_property_t *targets = obs_properties_add_list(props, k_source, obs_module_text("HUDMask.Source"),
							  OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_set_modified_callback2(targets, source_modified, ctx);

	const char *kind_val = "source";
	const char *canvas_val = "";
	if (ctx && ctx->self) {
		obs_data_t *settings = obs_source_get_settings(ctx->self);
		if (settings) {
			kind_val = obs_data_get_string(settings, k_kind);
			canvas_val = obs_data_get_string(settings, k_canvas);
			const bool scenes = kind_val && strcmp(kind_val, "scene") == 0;
			obs_property_set_visible(canvas, scenes);
			fill_target_list(targets, kind_val, canvas_val, ctx->self);
			obs_data_release(settings);
		} else {
			obs_property_set_visible(canvas, false);
			fill_target_list(targets, "source", "", ctx->self);
		}
	} else {
		obs_property_set_visible(canvas, false);
		fill_target_list(targets, "source", "", nullptr);
	}

	obs_property_t *same = obs_properties_add_text(props, k_same, obs_module_text("HUDMask.SameMasks"), OBS_TEXT_INFO);
	obs_property_set_visible(same, false);
	if (ctx && ctx->self) {
		obs_data_t *cur = obs_source_get_settings(ctx->self);
		if (cur) {
			fill_same_masks(props, ctx->self, cur);
			obs_data_release(cur);
		}
	}

	obs_properties_add_button2(props, "draw_mask", obs_module_text("HUDMask.DrawMask"), draw_mask_clicked, ctx);

	const bool has_mask = ctx && !ctx->mask_path.empty();
	obs_property_t *expand = obs_properties_add_int(props, k_expand, obs_module_text("HUDMask.Expand"), -48, 48, 1);
	obs_property_t *feather = obs_properties_add_int(props, k_feather, obs_module_text("HUDMask.Feather"), 0, 48, 1);
	obs_property_int_set_suffix(expand, " px");
	obs_property_int_set_suffix(feather, " px");
	obs_property_set_long_description(expand, obs_module_text("HUDMask.Expand.Help"));
	obs_property_set_long_description(feather, obs_module_text("HUDMask.Feather.Help"));
	obs_property_set_enabled(expand, has_mask);
	obs_property_set_enabled(feather, has_mask);

	obs_property_t *hide = obs_properties_add_bool(props, k_auto_hide, obs_module_text("HUDMask.AutoHide"));
	obs_property_set_long_description(hide, obs_module_text("HUDMask.AutoHide.Help"));
	obs_property_set_modified_callback2(hide, auto_hide_modified, ctx);
	obs_property_t *fade = obs_properties_add_int(props, k_fade, obs_module_text("HUDMask.AutoHide.Fade"), 0, 2000, 50);
	obs_property_int_set_suffix(fade, " ms");
	obs_property_set_long_description(fade, obs_module_text("HUDMask.AutoHide.Fade.Help"));
	obs_property_t *match = obs_properties_add_int_slider(props, k_match, obs_module_text("HUDMask.AutoHide.Match"), 0,
							     100, 1);
	obs_property_set_long_description(match, obs_module_text("HUDMask.AutoHide.Match.Help"));
	const bool hide_on = ctx && ctx->auto_hide;
	obs_property_set_enabled(hide, has_mask);
	obs_property_set_enabled(fade, has_mask && hide_on);
	obs_property_set_enabled(match, has_mask && hide_on);

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

void hud_mask_tick(void *data, float seconds)
{
	auto *ctx = static_cast<hud_mask *>(data);
	update_size(ctx);
	if (!ctx->auto_hide) {
		ctx->presence_shown = true;
		ctx->draw_alpha = 1.0f;
		ctx->presence_streak = 0;
		return;
	}
	hud_mask_presence_tick_fade(ctx, seconds);
}

void draw_texture(gs_texture_t *tex, uint32_t cx, uint32_t cy, gs_texture_t *mask, float opacity)
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
		gs_eparam_t *op = gs_effect_get_param_by_name(effect, "opacity");
		if (op)
			gs_effect_set_float(op, opacity);
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
	if (ctx->auto_hide && ctx->draw_alpha <= 0.001f)
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
	gs_texture_t *mask = ctx->mask_loaded ? ctx->mask_tex : nullptr;
	const float opacity = ctx->auto_hide ? ctx->draw_alpha : 1.0f;
	draw_texture(tex, ctx->cx, ctx->cy, mask, opacity);

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

obs_source_t *hud_mask_get_target(hud_mask *ctx)
{
	return ctx ? acquire_target(ctx) : nullptr;
}

void hud_mask_set_cutout(hud_mask *ctx, const char *path, int left, int top, int right, int bottom)
{
	if (!ctx || !ctx->self)
		return;

	if (!path || !path[0])
		hud_mask_presence_clear_ref(ctx);

	obs_data_t *settings = obs_source_get_settings(ctx->self);
	obs_data_set_string(settings, k_mask_path, path ? path : "");
	obs_data_set_int(settings, k_crop_left, left);
	obs_data_set_int(settings, k_crop_top, top);
	obs_data_set_int(settings, k_crop_right, right);
	obs_data_set_int(settings, k_crop_bottom, bottom);
	obs_source_update(ctx->self, settings);
	obs_data_release(settings);
}

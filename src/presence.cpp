/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "presence.hpp"
#include "hud-mask.hpp"
#include "mask-process.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>
#include <util/platform.h>
#include <plugin-support.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr int k_band_radius = 3;
constexpr int k_hysteresis = 3;
constexpr int k_ds_max = 640;
constexpr float k_cycle_sec = 0.10f;
constexpr const char k_ref_magic[6] = {'H', 'M', 'R', 'E', 'F', '1'};

std::mutex g_mu;
std::vector<hud_mask *> g_list;
bool g_started = false;
float g_wait = 0.0f;

struct Job {
	obs_weak_source_t *weak = nullptr;
	std::vector<hud_mask *> members;
	gs_texrender_t *texrender = nullptr;
	gs_stagesurf_t *stage = nullptr;
	uint32_t src_cx = 0;
	uint32_t src_cy = 0;
	uint32_t ds_cx = 0;
	uint32_t ds_cy = 0;
	int step = 0;
	bool held_showing = false;
};

Job *g_job = nullptr;

std::string ref_path_for_mask(const std::string &mask_path)
{
	if (mask_path.empty())
		return {};
	std::string p = mask_path;
	bool png = false;
	if (p.size() >= 4) {
		const char *e = p.c_str() + p.size() - 4;
		png = e[0] == '.' && (e[1] == 'p' || e[1] == 'P') && (e[2] == 'n' || e[2] == 'N') &&
		      (e[3] == 'g' || e[3] == 'G');
	}
	if (png)
		p.replace(p.size() - 4, 4, ".ref");
	else
		p += ".ref";
	return p;
}

void destroy_job(Job *job)
{
	if (!job)
		return;
	if (job->held_showing && job->weak) {
		obs_source_t *s = obs_weak_source_get_source(job->weak);
		if (s) {
			obs_source_dec_showing(s);
			obs_source_release(s);
		}
	}
	if (job->stage)
		gs_stagesurface_destroy(job->stage);
	if (job->texrender)
		gs_texrender_destroy(job->texrender);
	if (job->weak)
		obs_weak_source_release(job->weak);
	delete job;
}

void apply_score(hud_mask *ctx, float score)
{
	const float thresh = mask_presence_threshold(ctx->match_pct);
	const bool want = score >= thresh;
	if (want == ctx->presence_shown) {
		ctx->presence_streak = 0;
		return;
	}
	ctx->presence_streak++;
	if (ctx->presence_streak >= k_hysteresis) {
		ctx->presence_shown = want;
		ctx->presence_streak = 0;
	}
}

void learn_from_luma(hud_mask *ctx, const std::vector<uint8_t> &luma, int w, int h)
{
	if (!ctx || w < 1 || h < 1)
		return;
	std::vector<uint8_t> band;
	if (ctx->mask_w == w && ctx->mask_h == h && !ctx->mask_gray.empty())
		mask_silhouette_band(ctx->mask_gray, w, h, k_band_radius, band);
	else
		return;
	ctx->ref_luma = luma;
	ctx->ref_band = std::move(band);
	ctx->ref_w = w;
	ctx->ref_h = h;
	ctx->ref_valid = true;
	ctx->ref_capture_pending = false;
	hud_mask_presence_save_ref(ctx, ctx->ref_luma.data(), ctx->mask_gray.data(), w, h);
}

void score_member(hud_mask *ctx, const uint8_t *rgba, uint32_t linesize, uint32_t ds_cx, uint32_t ds_cy,
		  uint32_t src_cx, uint32_t src_cy)
{
	if (!ctx || !rgba || src_cx < 1 || src_cy < 1 || ds_cx < 1 || ds_cy < 1)
		return;
	if (ctx->cx < 1 || ctx->cy < 1)
		return;

	const float sx = static_cast<float>(ds_cx) / static_cast<float>(src_cx);
	const float sy = static_cast<float>(ds_cy) / static_cast<float>(src_cy);
	const int x0 = std::max(0, static_cast<int>(ctx->crop_left * sx));
	const int y0 = std::max(0, static_cast<int>(ctx->crop_top * sy));
	const int rw = std::max(1, static_cast<int>(ctx->cx * sx));
	const int rh = std::max(1, static_cast<int>(ctx->cy * sy));

	std::vector<uint8_t> roi(static_cast<size_t>(rw) * rh);
	for (int y = 0; y < rh; y++) {
		const int syi = std::min(static_cast<int>(ds_cy) - 1, y0 + y);
		const uint8_t *row = rgba + static_cast<size_t>(syi) * linesize;
		for (int x = 0; x < rw; x++) {
			const int sxi = std::min(static_cast<int>(ds_cx) - 1, x0 + x);
			const uint8_t *px = row + sxi * 4;
			roi[static_cast<size_t>(y) * rw + x] =
				static_cast<uint8_t>((77 * px[0] + 150 * px[1] + 29 * px[2]) >> 8);
		}
	}

	if (!ctx->ref_valid) {
		if (ctx->mask_w < 1 || ctx->mask_h < 1)
			return;
		std::vector<uint8_t> luma;
		mask_resize_luma(roi, rw, rh, luma, ctx->mask_w, ctx->mask_h);
		learn_from_luma(ctx, luma, ctx->mask_w, ctx->mask_h);
		ctx->presence_shown = true;
		ctx->presence_streak = 0;
		return;
	}

	std::vector<uint8_t> cur;
	mask_resize_luma(roi, rw, rh, cur, ctx->ref_w, ctx->ref_h);
	float score = 0;
	if (!mask_presence_score(ctx->ref_luma, cur, ctx->ref_band, ctx->ref_w, ctx->ref_h, &score))
		return;
	apply_score(ctx, score);
}

void finish_job_on_graphics()
{
	Job *job = g_job;
	if (!job)
		return;

	std::vector<hud_mask *> members;
	{
		std::lock_guard<std::mutex> lock(g_mu);
		members = job->members;
	}

	std::vector<uint8_t> packed;
	uint32_t linesize = job->ds_cx * 4;
	uint8_t *data = nullptr;
	uint32_t map_linesize = 0;
	if (job->stage && gs_stagesurface_map(job->stage, &data, &map_linesize) && data) {
		packed.resize(static_cast<size_t>(job->ds_cy) * linesize);
		for (uint32_t y = 0; y < job->ds_cy; y++)
			memcpy(packed.data() + static_cast<size_t>(y) * linesize, data + static_cast<size_t>(y) * map_linesize,
			       linesize);
		gs_stagesurface_unmap(job->stage);
	}

	const uint32_t ds_cx = job->ds_cx;
	const uint32_t ds_cy = job->ds_cy;
	const uint32_t src_cx = job->src_cx;
	const uint32_t src_cy = job->src_cy;
	destroy_job(job);
	g_job = nullptr;

	obs_leave_graphics();
	if (!packed.empty()) {
		for (hud_mask *ctx : members)
			score_member(ctx, packed.data(), linesize, ds_cx, ds_cy, src_cx, src_cy);
	}
	obs_enter_graphics();
	g_wait = k_cycle_sec;
}

bool start_job()
{
	std::map<std::string, std::vector<hud_mask *>> groups;
	{
		std::lock_guard<std::mutex> lock(g_mu);
		for (hud_mask *ctx : g_list) {
			if (!ctx || !ctx->self || !ctx->auto_hide)
				continue;
			if (!obs_source_showing(ctx->self))
				continue;
			if (ctx->mask_path.empty())
				continue;
			if (!ctx->ref_valid && !ctx->ref_capture_pending)
				continue;
			if (ctx->target_name.empty())
				continue;
			groups[ctx->target_name].push_back(ctx);
		}
	}
	if (groups.empty())
		return false;

	auto it = groups.begin();
	hud_mask *lead = it->second.front();
	obs_source_t *target = hud_mask_get_target(lead);
	if (!target)
		return false;

	const uint32_t src_cx = obs_source_get_base_width(target);
	const uint32_t src_cy = obs_source_get_base_height(target);
	if (!src_cx || !src_cy) {
		obs_source_release(target);
		return false;
	}

	uint32_t ds_cx = src_cx;
	uint32_t ds_cy = src_cy;
	if (src_cx >= src_cy) {
		ds_cx = std::min(static_cast<uint32_t>(k_ds_max), src_cx);
		ds_cy = std::max(1u, src_cy * ds_cx / src_cx);
	} else {
		ds_cy = std::min(static_cast<uint32_t>(k_ds_max), src_cy);
		ds_cx = std::max(1u, src_cx * ds_cy / src_cy);
	}

	Job *job = new Job();
	job->weak = obs_source_get_weak_source(target);
	job->members = it->second;
	job->src_cx = src_cx;
	job->src_cy = src_cy;
	job->ds_cx = ds_cx;
	job->ds_cy = ds_cy;
	obs_source_release(target);

	g_job = job;
	return true;
}

void presence_tick(void *, float seconds)
{
	if (g_job) {
		obs_enter_graphics();
		Job *job = g_job;
		if (job->step == 0) {
			obs_source_t *source = obs_weak_source_get_source(job->weak);
			if (!source) {
				destroy_job(job);
				g_job = nullptr;
				obs_leave_graphics();
				g_wait = k_cycle_sec;
				return;
			}
			if (!job->texrender)
				job->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
			if (!job->stage)
				job->stage = gs_stagesurface_create(job->ds_cx, job->ds_cy, GS_RGBA);
			gs_texrender_reset(job->texrender);
			if (gs_texrender_begin(job->texrender, job->ds_cx, job->ds_cy)) {
				struct vec4 zero;
				vec4_zero(&zero);
				gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
				gs_ortho(0.0f, (float)job->src_cx, 0.0f, (float)job->src_cy, -100.0f, 100.0f);
				gs_blend_state_push();
				gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
				if (!job->held_showing) {
					obs_source_inc_showing(source);
					job->held_showing = true;
				}
				obs_source_video_render(source);
				gs_blend_state_pop();
				gs_texrender_end(job->texrender);
			}
			obs_source_release(source);
			job->step = 1;
		} else if (job->step == 1) {
			gs_texture_t *tex = job->texrender ? gs_texrender_get_texture(job->texrender) : nullptr;
			if (tex && job->stage)
				gs_stage_texture(job->stage, tex);
			job->step = 2;
		} else {
			finish_job_on_graphics();
		}
		obs_leave_graphics();
		return;
	}

	g_wait -= seconds;
	if (g_wait > 0.0f)
		return;
	if (!start_job())
		g_wait = k_cycle_sec;
}

} // namespace

void hud_mask_presence_start(void)
{
	if (g_started)
		return;
	g_started = true;
	g_wait = 0.0f;
	obs_add_tick_callback(presence_tick, nullptr);
}

void hud_mask_presence_stop(void)
{
	if (!g_started)
		return;
	g_started = false;
	obs_remove_tick_callback(presence_tick, nullptr);
	if (g_job) {
		obs_enter_graphics();
		destroy_job(g_job);
		obs_leave_graphics();
		g_job = nullptr;
	}
	std::lock_guard<std::mutex> lock(g_mu);
	g_list.clear();
}

void hud_mask_presence_register(hud_mask *ctx)
{
	if (!ctx)
		return;
	std::lock_guard<std::mutex> lock(g_mu);
	if (std::find(g_list.begin(), g_list.end(), ctx) == g_list.end())
		g_list.push_back(ctx);
}

void hud_mask_presence_unregister(hud_mask *ctx)
{
	if (!ctx)
		return;
	std::lock_guard<std::mutex> lock(g_mu);
	g_list.erase(std::remove(g_list.begin(), g_list.end(), ctx), g_list.end());
	if (g_job) {
		auto &m = g_job->members;
		m.erase(std::remove(m.begin(), m.end(), ctx), m.end());
	}
}

void hud_mask_presence_clear_ref(hud_mask *ctx)
{
	if (!ctx)
		return;
	if (!ctx->mask_path.empty()) {
		const std::string path = ref_path_for_mask(ctx->mask_path);
		if (!path.empty())
			os_unlink(path.c_str());
	}
	ctx->ref_valid = false;
	ctx->ref_capture_pending = false;
	ctx->ref_w = ctx->ref_h = 0;
	ctx->ref_luma.clear();
	ctx->ref_band.clear();
}

void hud_mask_presence_load_ref(hud_mask *ctx)
{
	if (!ctx)
		return;
	ctx->ref_valid = false;
	ctx->ref_luma.clear();
	ctx->ref_band.clear();
	ctx->ref_w = ctx->ref_h = 0;
	if (ctx->mask_path.empty())
		return;

	const std::string path = ref_path_for_mask(ctx->mask_path);
	FILE *f = os_fopen(path.c_str(), "rb");
	if (!f)
		return;
	char magic[6];
	uint32_t wh[2] = {0, 0};
	if (fread(magic, 1, 6, f) != 6 || memcmp(magic, k_ref_magic, 6) != 0 || fread(wh, 4, 2, f) != 2) {
		fclose(f);
		return;
	}
	const int w = static_cast<int>(wh[0]);
	const int h = static_cast<int>(wh[1]);
	if (w < 1 || h < 1 || w > 8192 || h > 8192) {
		fclose(f);
		return;
	}
	const size_t n = static_cast<size_t>(w) * h;
	std::vector<uint8_t> luma(n);
	std::vector<uint8_t> band(n);
	if (fread(luma.data(), 1, n, f) != n || fread(band.data(), 1, n, f) != n) {
		fclose(f);
		return;
	}
	fclose(f);
	ctx->ref_luma = std::move(luma);
	ctx->ref_band = std::move(band);
	ctx->ref_w = w;
	ctx->ref_h = h;
	ctx->ref_valid = true;
	ctx->ref_capture_pending = false;
}

bool hud_mask_presence_save_ref(hud_mask *ctx, const uint8_t *luma, const uint8_t *mask_gray, int width, int height)
{
	if (!ctx || !luma || !mask_gray || width < 1 || height < 1)
		return false;

	std::vector<uint8_t> mask(static_cast<size_t>(width) * height);
	memcpy(mask.data(), mask_gray, mask.size());
	std::vector<uint8_t> band;
	mask_silhouette_band(mask, width, height, k_band_radius, band);
	std::vector<uint8_t> lum(static_cast<size_t>(width) * height);
	memcpy(lum.data(), luma, lum.size());

	ctx->ref_luma = lum;
	ctx->ref_band = band;
	ctx->ref_w = width;
	ctx->ref_h = height;
	ctx->ref_valid = true;
	ctx->ref_capture_pending = false;

	if (ctx->mask_path.empty())
		return true;
	const std::string path = ref_path_for_mask(ctx->mask_path);
	FILE *f = os_fopen(path.c_str(), "wb");
	if (!f)
		return false;
	const uint32_t wh[2] = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	const bool ok = fwrite(k_ref_magic, 1, 6, f) == 6 && fwrite(wh, 4, 2, f) == 2 &&
			fwrite(lum.data(), 1, lum.size(), f) == lum.size() &&
			fwrite(band.data(), 1, band.size(), f) == band.size();
	fclose(f);
	return ok;
}

void hud_mask_presence_tick_fade(hud_mask *ctx, float seconds)
{
	if (!ctx)
		return;
	const float target = ctx->presence_shown ? 1.0f : 0.0f;
	if (ctx->fade_ms <= 0) {
		ctx->draw_alpha = target;
		return;
	}
	const float speed = 1000.0f / static_cast<float>(ctx->fade_ms);
	if (ctx->draw_alpha < target)
		ctx->draw_alpha = std::min(target, ctx->draw_alpha + speed * seconds);
	else if (ctx->draw_alpha > target)
		ctx->draw_alpha = std::max(target, ctx->draw_alpha - speed * seconds);
}

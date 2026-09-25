/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "magic-sam.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "third_party/onnxruntime/onnxruntime_c_api.h"

namespace {

constexpr int k_pad = 32;
constexpr int k_min_crop = 8;

struct Sessions {
	const OrtApi *api = nullptr;
	HMODULE dll = nullptr;
	OrtEnv *env = nullptr;
	OrtSession *encoder = nullptr;
	OrtSession *decoder = nullptr;
	OrtMemoryInfo *memory = nullptr;
	bool ready = false;
	bool failed = false;
};

Sessions g_sam;
std::mutex g_mu;

bool ort_ok(const OrtApi *api, OrtStatus *st, const char *what)
{
	if (!st)
		return true;
	const char *msg = api->GetErrorMessage(st);
	obs_log(LOG_WARNING, "MobileSAM %s: %s", what, msg ? msg : "error");
	api->ReleaseStatus(st);
	return false;
}

std::wstring widen(const char *utf8)
{
	if (!utf8 || !utf8[0])
		return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (n <= 0)
		return {};
	std::wstring out(static_cast<size_t>(n), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
	if (!out.empty() && out.back() == L'\0')
		out.pop_back();
	return out;
}

std::wstring module_file_w(const char *rel)
{
	char *path = obs_module_file(rel);
	if (!path)
		return {};
	std::wstring wide = widen(path);
	bfree(path);
	return wide;
}

HMODULE load_ort()
{
	wchar_t path[MAX_PATH];
	HMODULE self = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&load_ort), &self))
		return nullptr;
	const DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
	if (!n || n >= MAX_PATH)
		return nullptr;
	wchar_t *slash = wcsrchr(path, L'\\');
	if (!slash)
		return nullptr;
	*(slash + 1) = 0;
	if (wcslen(path) + wcslen(L"onnxruntime.dll") + 1 >= MAX_PATH)
		return nullptr;
	wcscat_s(path, L"onnxruntime.dll");
	return LoadLibraryW(path);
}

bool open_session(const OrtApi *api, OrtEnv *env, OrtSessionOptions *opts, const char *rel, OrtSession **out)
{
	const std::wstring path = module_file_w(rel);
	if (path.empty())
		return false;
	if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;
	return ort_ok(api, api->CreateSession(env, path.c_str(), opts, out), rel);
}

bool ensure_sessions()
{
	if (g_sam.ready)
		return true;
	if (g_sam.failed)
		return false;

	g_sam.dll = load_ort();
	if (!g_sam.dll) {
		obs_log(LOG_INFO, "MobileSAM skipped: onnxruntime.dll is not next to the plugin");
		g_sam.failed = true;
		return false;
	}
	auto get_base = reinterpret_cast<const OrtApiBase *(ORT_API_CALL *)(void)>(GetProcAddress(g_sam.dll, "OrtGetApiBase"));
	if (!get_base) {
		obs_log(LOG_WARNING, "MobileSAM skipped: OrtGetApiBase missing");
		g_sam.failed = true;
		return false;
	}
	g_sam.api = get_base()->GetApi(ORT_API_VERSION);
	if (!g_sam.api) {
		obs_log(LOG_WARNING, "MobileSAM skipped: ONNX Runtime API %d is not supported", ORT_API_VERSION);
		g_sam.failed = true;
		return false;
	}
	const OrtApi *api = g_sam.api;
	if (!ort_ok(api, api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "hudmask", &g_sam.env), "CreateEnv")) {
		g_sam.failed = true;
		return false;
	}

	OrtSessionOptions *opts = nullptr;
	if (!ort_ok(api, api->CreateSessionOptions(&opts), "CreateSessionOptions")) {
		g_sam.failed = true;
		return false;
	}
	if (OrtStatus *st = api->SetIntraOpNumThreads(opts, 2))
		api->ReleaseStatus(st);
	if (OrtStatus *st = api->SetSessionGraphOptimizationLevel(opts, ORT_ENABLE_ALL))
		api->ReleaseStatus(st);

	using AppendDml = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, int);
	auto append_dml = reinterpret_cast<AppendDml>(GetProcAddress(g_sam.dll, "OrtSessionOptionsAppendExecutionProvider_DML"));
	bool dml = false;
	if (append_dml) {
		OrtStatus *st = append_dml(opts, 0);
		if (!st)
			dml = true;
		else
			api->ReleaseStatus(st);
	}

	const bool enc = open_session(api, g_sam.env, opts, "models/mobilesam.encoder.onnx", &g_sam.encoder);
	const bool dec = enc && open_session(api, g_sam.env, opts, "models/mobilesam.decoder.onnx", &g_sam.decoder);
	api->ReleaseSessionOptions(opts);
	if (!enc || !dec) {
		static bool logged = false;
		if (!logged) {
			obs_log(LOG_INFO, "MobileSAM skipped: models/mobilesam.encoder.onnx and decoder.onnx are not in plugin data");
			logged = true;
		}
		return false;
	}
	if (!ort_ok(api, api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &g_sam.memory), "CreateCpuMemoryInfo")) {
		g_sam.failed = true;
		return false;
	}
	g_sam.ready = true;
	obs_log(LOG_INFO, "MobileSAM ready (%s)", dml ? "DirectML" : "CPU");
	return true;
}

OrtValue *tensor(const OrtApi *api, OrtMemoryInfo *mem, void *data, size_t bytes, const int64_t *shape, size_t rank)
{
	OrtValue *v = nullptr;
	if (!ort_ok(api, api->CreateTensorWithDataAsOrtValue(mem, data, bytes, shape, rank, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &v),
		    "CreateTensor"))
		return nullptr;
	return v;
}

bool tensor_shape(const OrtApi *api, OrtValue *v, std::vector<int64_t> &dims)
{
	OrtTensorTypeAndShapeInfo *info = nullptr;
	if (!ort_ok(api, api->GetTensorTypeAndShape(v, &info), "GetTensorTypeAndShape"))
		return false;
	size_t n = 0;
	const bool ok = ort_ok(api, api->GetDimensionsCount(info, &n), "GetDimensionsCount");
	dims.assign(n, 0);
	const bool dims_ok = ok && ort_ok(api, api->GetDimensions(info, dims.data(), n), "GetDimensions");
	api->ReleaseTensorTypeAndShapeInfo(info);
	return dims_ok;
}

int sam_scaled_dim(int size, int longest)
{
	if (longest < 1)
		return size;
	return std::max(1, static_cast<int>(std::floor(static_cast<float>(size) * (1024.f / static_cast<float>(longest)) + 0.5f)));
}

float sample_chan(const std::vector<float> &src, int sw, int sh, float x, float y, int c)
{
	x = std::clamp(x, 0.f, static_cast<float>(sw - 1));
	y = std::clamp(y, 0.f, static_cast<float>(sh - 1));
	const int x0 = static_cast<int>(std::floor(x));
	const int y0 = static_cast<int>(std::floor(y));
	const int x1 = std::min(x0 + 1, sw - 1);
	const int y1 = std::min(y0 + 1, sh - 1);
	const float tx = x - static_cast<float>(x0);
	const float ty = y - static_cast<float>(y0);
	auto at = [&](int px, int py) { return src[(static_cast<size_t>(py) * sw + px) * 3 + c]; };
	const float a = at(x0, y0) * (1.f - tx) + at(x1, y0) * tx;
	const float b = at(x0, y1) * (1.f - tx) + at(x1, y1) * tx;
	return a * (1.f - ty) + b * ty;
}

void resize_longest_1024(std::vector<float> &image, int sw, int sh, int &dw, int &dh)
{
	const int longest = std::max(sw, sh);
	dw = sam_scaled_dim(sw, longest);
	dh = sam_scaled_dim(sh, longest);
	if (dw == sw && dh == sh)
		return;
	std::vector<float> dst(static_cast<size_t>(dw) * dh * 3);
	for (int y = 0; y < dh; y++) {
		const float sy = (static_cast<float>(y) + 0.5f) * static_cast<float>(sh) / static_cast<float>(dh) - 0.5f;
		for (int x = 0; x < dw; x++) {
			const float sx = (static_cast<float>(x) + 0.5f) * static_cast<float>(sw) / static_cast<float>(dw) - 0.5f;
			float *out = dst.data() + (static_cast<size_t>(y) * dw + x) * 3;
			out[0] = sample_chan(image, sw, sh, sx, sy, 0);
			out[1] = sample_chan(image, sw, sh, sx, sy, 1);
			out[2] = sample_chan(image, sw, sh, sx, sy, 2);
		}
	}
	image.swap(dst);
}

} // namespace

MagicSamPick magic_sam_select(const uint8_t *bgra, int stride, int width, int height, const std::vector<MaskPoint> &loop)
{
	MagicSamPick pick;
	pick.width = width;
	pick.height = height;
	if (!bgra || stride < width * 4 || width < k_min_crop || height < k_min_crop || loop.size() < 3) {
		pick.status = MagicSamStatus::Rejected;
		return pick;
	}

	std::lock_guard<std::mutex> lock(g_mu);
	if (!ensure_sessions()) {
		pick.status = MagicSamStatus::Unavailable;
		return pick;
	}
	const OrtApi *api = g_sam.api;

	const MaskLoopBounds crop = mask_loop_bounds(loop, width, height, k_pad);
	if (crop.empty || crop.width < k_min_crop || crop.height < k_min_crop) {
		pick.status = MagicSamStatus::Rejected;
		return pick;
	}

	std::vector<float> image(static_cast<size_t>(crop.width) * crop.height * 3);
	for (int y = 0; y < crop.height; y++) {
		const uint8_t *row = bgra + static_cast<size_t>(crop.y + y) * stride + static_cast<size_t>(crop.x) * 4;
		float *dst = image.data() + static_cast<size_t>(y) * crop.width * 3;
		for (int x = 0; x < crop.width; x++) {
			dst[x * 3 + 0] = row[x * 4 + 2];
			dst[x * 3 + 1] = row[x * 4 + 1];
			dst[x * 3 + 2] = row[x * 4 + 0];
		}
	}

	int enc_w = crop.width;
	int enc_h = crop.height;
	resize_longest_1024(image, crop.width, crop.height, enc_w, enc_h);
	const int64_t image_shape[3] = {enc_h, enc_w, 3};
	OrtValue *image_v = tensor(api, g_sam.memory, image.data(), image.size() * sizeof(float), image_shape, 3);
	if (!image_v) {
		pick.status = MagicSamStatus::Failed;
		return pick;
	}
	const char *enc_in[] = {"input_image"};
	const char *enc_out[] = {"image_embeddings"};
	OrtValue *emb = nullptr;
	const bool enc_ok = ort_ok(api, api->Run(g_sam.encoder, nullptr, enc_in, &image_v, 1, enc_out, 1, &emb), "encoder");
	api->ReleaseValue(image_v);
	if (!enc_ok || !emb) {
		if (emb)
			api->ReleaseValue(emb);
		pick.status = MagicSamStatus::Failed;
		return pick;
	}

	float min_x = loop[0].x;
	float min_y = loop[0].y;
	float max_x = loop[0].x;
	float max_y = loop[0].y;
	for (const MaskPoint &pt : loop) {
		min_x = std::min(min_x, pt.x);
		min_y = std::min(min_y, pt.y);
		max_x = std::max(max_x, pt.x);
		max_y = std::max(max_y, pt.y);
	}
	float box[4];
	mask_sam_box_to_input(min_x - static_cast<float>(crop.x), min_y - static_cast<float>(crop.y),
			      max_x - static_cast<float>(crop.x), max_y - static_cast<float>(crop.y), crop.width, crop.height, box);
	float coords[4] = {box[0], box[1], box[2], box[3]};
	float labels[2] = {2.f, 3.f};
	std::vector<float> mask_in(256 * 256, 0.f);
	float has_mask = 0.f;
	float orig[2] = {static_cast<float>(crop.height), static_cast<float>(crop.width)};
	const int64_t coord_shape[3] = {1, 2, 2};
	const int64_t label_shape[2] = {1, 2};
	const int64_t mask_shape[4] = {1, 1, 256, 256};
	const int64_t has_shape[1] = {1};
	const int64_t orig_shape[1] = {2};

	OrtValue *coord_v = tensor(api, g_sam.memory, coords, sizeof(coords), coord_shape, 3);
	OrtValue *label_v = tensor(api, g_sam.memory, labels, sizeof(labels), label_shape, 2);
	OrtValue *mask_v = tensor(api, g_sam.memory, mask_in.data(), mask_in.size() * sizeof(float), mask_shape, 4);
	OrtValue *has_v = tensor(api, g_sam.memory, &has_mask, sizeof(has_mask), has_shape, 1);
	OrtValue *orig_v = tensor(api, g_sam.memory, orig, sizeof(orig), orig_shape, 1);
	if (!coord_v || !label_v || !mask_v || !has_v || !orig_v) {
		api->ReleaseValue(emb);
		if (coord_v)
			api->ReleaseValue(coord_v);
		if (label_v)
			api->ReleaseValue(label_v);
		if (mask_v)
			api->ReleaseValue(mask_v);
		if (has_v)
			api->ReleaseValue(has_v);
		if (orig_v)
			api->ReleaseValue(orig_v);
		pick.status = MagicSamStatus::Failed;
		return pick;
	}

	const char *dec_in[] = {"image_embeddings", "point_coords", "point_labels", "mask_input", "has_mask_input", "orig_im_size"};
	const char *dec_out_names[] = {"masks", "iou_predictions"};
	OrtValue *dec_in_v[] = {emb, coord_v, label_v, mask_v, has_v, orig_v};
	OrtValue *dec_out[2] = {nullptr, nullptr};
	const bool dec_ok = ort_ok(api, api->Run(g_sam.decoder, nullptr, dec_in, dec_in_v, 6, dec_out_names, 2, dec_out), "decoder");
	api->ReleaseValue(emb);
	api->ReleaseValue(coord_v);
	api->ReleaseValue(label_v);
	api->ReleaseValue(mask_v);
	api->ReleaseValue(has_v);
	api->ReleaseValue(orig_v);
	if (!dec_ok || !dec_out[0] || !dec_out[1]) {
		api->ReleaseValue(dec_out[0]);
		api->ReleaseValue(dec_out[1]);
		pick.status = MagicSamStatus::Failed;
		return pick;
	}

	float *iou_p = nullptr;
	float *mask_p = nullptr;
	std::vector<int64_t> mask_dims;
	const bool read_ok = ort_ok(api, api->GetTensorMutableData(dec_out[1], reinterpret_cast<void **>(&iou_p)), "iou") &&
			     ort_ok(api, api->GetTensorMutableData(dec_out[0], reinterpret_cast<void **>(&mask_p)), "mask") &&
			     tensor_shape(api, dec_out[0], mask_dims);
	if (!read_ok || !iou_p || !mask_p || mask_dims.size() < 2) {
		api->ReleaseValue(dec_out[0]);
		api->ReleaseValue(dec_out[1]);
		pick.status = MagicSamStatus::Failed;
		return pick;
	}
	const int mask_h = static_cast<int>(mask_dims[mask_dims.size() - 2]);
	const int mask_w = static_cast<int>(mask_dims[mask_dims.size() - 1]);
	pick.iou = iou_p[0];
	if (mask_w != crop.width || mask_h != crop.height) {
		api->ReleaseValue(dec_out[0]);
		api->ReleaseValue(dec_out[1]);
		obs_log(LOG_WARNING, "MobileSAM mask %dx%d does not match crop %dx%d", mask_w, mask_h, crop.width, crop.height);
		pick.status = MagicSamStatus::Failed;
		return pick;
	}

	int raw_count = 0;
	std::vector<uint8_t> full(static_cast<size_t>(width) * height, 0);
	for (int y = 0; y < crop.height; y++) {
		for (int x = 0; x < crop.width; x++) {
			if (mask_p[static_cast<size_t>(y) * crop.width + x] <= 0.f)
				continue;
			raw_count++;
			full[static_cast<size_t>(crop.y + y) * width + (crop.x + x)] = 255;
		}
	}
	api->ReleaseValue(dec_out[0]);
	api->ReleaseValue(dec_out[1]);

	if (!mask_magic_pick_ok(raw_count, crop.width * crop.height, pick.iou)) {
		pick.status = MagicSamStatus::Rejected;
		return pick;
	}
	mask_clip_to_loop(full, width, height, loop);
	int clipped = 0;
	for (uint8_t p : full)
		if (p >= 128)
			clipped++;
	if (clipped < 16) {
		pick.status = MagicSamStatus::Rejected;
		return pick;
	}
	pick.gray = std::move(full);
	pick.status = MagicSamStatus::Ok;
	return pick;
}

#include "mask-process.hpp"

#include <algorithm>
#include <cmath>

void mask_binarize(std::vector<uint8_t> &gray, uint8_t threshold)
{
	for (uint8_t &p : gray)
		p = p >= threshold ? 255 : 0;
}

void mask_expand(std::vector<uint8_t> &gray, int width, int height, int radius)
{
	if (radius == 0 || width <= 0 || height <= 0)
		return;
	if (static_cast<int>(gray.size()) < width * height)
		return;

	const bool dilate = radius > 0;
	const int r = std::abs(radius);
	const std::vector<uint8_t> src = gray;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			bool any = false;
			bool all = true;
			for (int dy = -r; dy <= r; dy++) {
				for (int dx = -r; dx <= r; dx++) {
					if (dx * dx + dy * dy > r * r)
						continue;
					const int nx = x + dx;
					const int ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
						all = false;
						continue;
					}
					if (src[static_cast<size_t>(ny) * width + nx] >= 128)
						any = true;
					else
						all = false;
				}
			}
			gray[static_cast<size_t>(y) * width + x] = (dilate ? any : all) ? 255 : 0;
		}
	}
}

void mask_feather(std::vector<uint8_t> &gray, int width, int height, int radius)
{
	if (radius <= 0 || width <= 0 || height <= 0)
		return;
	if (static_cast<int>(gray.size()) < width * height)
		return;

	const int n = width * height;
	std::vector<int> dist(static_cast<size_t>(n), 9999);
	std::vector<int> queue;
	queue.reserve(static_cast<size_t>(n));

	for (int i = 0; i < n; i++) {
		if (gray[static_cast<size_t>(i)] < 128) {
			dist[static_cast<size_t>(i)] = 0;
			queue.push_back(i);
		}
	}

	for (size_t qi = 0; qi < queue.size(); qi++) {
		const int i = queue[qi];
		const int nd = dist[static_cast<size_t>(i)] + 1;
		if (nd > radius)
			continue;
		const int x = i % width;
		const int y = i / width;
		const int nb[4] = {x > 0 ? i - 1 : -1, x + 1 < width ? i + 1 : -1, y > 0 ? i - width : -1,
				   y + 1 < height ? i + width : -1};
		for (int nbi : nb) {
			if (nbi < 0)
				continue;
			if (dist[static_cast<size_t>(nbi)] <= nd)
				continue;
			dist[static_cast<size_t>(nbi)] = nd;
			queue.push_back(nbi);
		}
	}

	for (int i = 0; i < n; i++) {
		const int d = dist[static_cast<size_t>(i)];
		if (d == 0)
			gray[static_cast<size_t>(i)] = 0;
		else if (d >= radius)
			gray[static_cast<size_t>(i)] = 255;
		else
			gray[static_cast<size_t>(i)] = static_cast<uint8_t>(d * 255 / radius);
	}
}

bool mask_is_roundish(int bbox_w, int bbox_h, int filled_pixels)
{
	if (bbox_w < 1 || bbox_h < 1 || filled_pixels < 1)
		return false;
	const double aspect = static_cast<double>(std::min(bbox_w, bbox_h)) / std::max(bbox_w, bbox_h);
	const double fill = static_cast<double>(filled_pixels) / (bbox_w * bbox_h);
	return aspect >= 0.78 && fill >= 0.52;
}

int mask_circle_radius_from_bounds(int bbox_w, int bbox_h)
{
	return std::max(6, std::min(bbox_w, bbox_h) / 2);
}

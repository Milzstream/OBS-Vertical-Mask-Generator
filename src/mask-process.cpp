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

/* wall = pixels >= 20. closed = wall plus empty pixels that have painted
 * neighbors on opposite sides (a 1px hole). Does not thicken a solid outline. */
static bool mask_walls_and_gaps(const std::vector<uint8_t> &gray, int width, int height, std::vector<uint8_t> &wall,
				std::vector<uint8_t> &closed)
{
	if (width <= 0 || height <= 0)
		return false;
	if (static_cast<int>(gray.size()) < width * height)
		return false;

	const int n = width * height;
	wall.assign(static_cast<size_t>(n), 0);
	for (int i = 0; i < n; i++) {
		if (gray[static_cast<size_t>(i)] >= 20)
			wall[static_cast<size_t>(i)] = 1;
	}

	closed = wall;
	for (int yy = 1; yy < height - 1; yy++) {
		for (int xx = 1; xx < width - 1; xx++) {
			const int i = yy * width + xx;
			if (wall[static_cast<size_t>(i)])
				continue;
			const bool h = wall[static_cast<size_t>(i - 1)] && wall[static_cast<size_t>(i + 1)];
			const bool v = wall[static_cast<size_t>(i - width)] && wall[static_cast<size_t>(i + width)];
			const bool d1 = wall[static_cast<size_t>(i - width - 1)] &&
					wall[static_cast<size_t>(i + width + 1)];
			const bool d2 = wall[static_cast<size_t>(i - width + 1)] &&
					wall[static_cast<size_t>(i + width - 1)];
			if (h || v || d1 || d2)
				closed[static_cast<size_t>(i)] = 1;
		}
	}
	return true;
}

static bool mask_touches_seen(const std::vector<uint8_t> &seen, int width, int height, int x, int y)
{
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			if (dx == 0 && dy == 0)
				continue;
			const int nx = x + dx;
			const int ny = y + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height)
				continue;
			if (seen[static_cast<size_t>(ny) * width + nx])
				return true;
		}
	}
	return false;
}

bool mask_flood_fill(std::vector<uint8_t> &gray, int width, int height, int x, int y, bool absorb_outline)
{
	if (x < 0 || y < 0 || x >= width || y >= height)
		return false;

	std::vector<uint8_t> wall;
	std::vector<uint8_t> closed;
	if (!mask_walls_and_gaps(gray, width, height, wall, closed))
		return false;

	const int n = width * height;
	const int seed = y * width + x;
	if (closed[static_cast<size_t>(seed)])
		return false;

	std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
	std::vector<int> q;
	q.push_back(seed);
	seen[static_cast<size_t>(seed)] = 1;
	for (size_t qi = 0; qi < q.size(); qi++) {
		const int i = q[qi];
		const int px = i % width;
		const int py = i / width;
		gray[static_cast<size_t>(i)] = 255;
		const int nb[4] = {px > 0 ? i - 1 : -1, px + 1 < width ? i + 1 : -1, py > 0 ? i - width : -1,
				   py + 1 < height ? i + width : -1};
		for (int nbi : nb) {
			if (nbi < 0 || seen[static_cast<size_t>(nbi)])
				continue;
			if (closed[static_cast<size_t>(nbi)])
				continue;
			seen[static_cast<size_t>(nbi)] = 1;
			q.push_back(nbi);
		}
	}

	if (!absorb_outline)
		return true;

	for (int iter = 0; iter < 8; iter++) {
		std::vector<int> extra;
		for (int yy = 0; yy < height; yy++) {
			for (int xx = 0; xx < width; xx++) {
				const int i = yy * width + xx;
				if (seen[static_cast<size_t>(i)] || !closed[static_cast<size_t>(i)])
					continue;
				if (mask_touches_seen(seen, width, height, xx, yy))
					extra.push_back(i);
			}
		}
		if (extra.empty())
			break;
		for (int i : extra) {
			seen[static_cast<size_t>(i)] = 1;
			gray[static_cast<size_t>(i)] = 255;
		}
	}
	return true;
}

bool mask_flood_erase(std::vector<uint8_t> &gray, int width, int height, int x, int y)
{
	if (x < 0 || y < 0 || x >= width || y >= height)
		return false;

	std::vector<uint8_t> wall;
	std::vector<uint8_t> closed;
	if (!mask_walls_and_gaps(gray, width, height, wall, closed))
		return false;

	const int n = width * height;
	const int seed = y * width + x;
	if (!closed[static_cast<size_t>(seed)])
		return false;

	std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
	std::vector<int> q;
	q.push_back(seed);
	seen[static_cast<size_t>(seed)] = 1;
	for (size_t qi = 0; qi < q.size(); qi++) {
		const int i = q[qi];
		const int px = i % width;
		const int py = i / width;
		gray[static_cast<size_t>(i)] = 0;
		const int nb[4] = {px > 0 ? i - 1 : -1, px + 1 < width ? i + 1 : -1, py > 0 ? i - width : -1,
				   py + 1 < height ? i + width : -1};
		for (int nbi : nb) {
			if (nbi < 0 || seen[static_cast<size_t>(nbi)])
				continue;
			if (!closed[static_cast<size_t>(nbi)])
				continue;
			seen[static_cast<size_t>(nbi)] = 1;
			q.push_back(nbi);
		}
	}
	return true;
}

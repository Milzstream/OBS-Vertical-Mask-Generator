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

int mask_sobel(const std::vector<uint8_t> &lum, int width, int height, std::vector<uint16_t> &mag)
{
	const int n = width * height;
	mag.assign(static_cast<size_t>(n), 0);
	if (width < 3 || height < 3 || static_cast<int>(lum.size()) < n)
		return 1;

	int max_mag = 1;
	for (int y = 1; y < height - 1; y++) {
		for (int x = 1; x < width - 1; x++) {
			const int i = y * width + x;
			const int gx = -lum[static_cast<size_t>(i - width - 1)] - 2 * lum[static_cast<size_t>(i - 1)] -
				       lum[static_cast<size_t>(i + width - 1)] + lum[static_cast<size_t>(i - width + 1)] +
				       2 * lum[static_cast<size_t>(i + 1)] + lum[static_cast<size_t>(i + width + 1)];
			const int gy = -lum[static_cast<size_t>(i - width - 1)] - 2 * lum[static_cast<size_t>(i - width)] -
				       lum[static_cast<size_t>(i - width + 1)] + lum[static_cast<size_t>(i + width - 1)] +
				       2 * lum[static_cast<size_t>(i + width)] + lum[static_cast<size_t>(i + width + 1)];
			const int m = std::abs(gx) + std::abs(gy);
			mag[static_cast<size_t>(i)] = static_cast<uint16_t>(m);
			max_mag = std::max(max_mag, m);
		}
	}
	return max_mag;
}

MaskCrop mask_crop_from_opaque(const std::vector<uint8_t> &gray, int width, int height, uint8_t threshold, int pad)
{
	MaskCrop c;
	if (width <= 0 || height <= 0 || static_cast<int>(gray.size()) < width * height)
		return c;

	int minx = width, miny = height, maxx = -1, maxy = -1;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (gray[static_cast<size_t>(y) * width + x] < threshold)
				continue;
			minx = std::min(minx, x);
			miny = std::min(miny, y);
			maxx = std::max(maxx, x);
			maxy = std::max(maxy, y);
		}
	}
	if (maxx < minx)
		return c;

	if (pad < 0)
		pad = 0;
	minx = std::max(0, minx - pad);
	miny = std::max(0, miny - pad);
	maxx = std::min(width - 1, maxx + pad);
	maxy = std::min(height - 1, maxy + pad);

	c.min_x = minx;
	c.min_y = miny;
	c.max_x = maxx;
	c.max_y = maxy;
	c.left = minx;
	c.top = miny;
	c.right = width - 1 - maxx;
	c.bottom = height - 1 - maxy;
	c.empty = false;
	return c;
}

static void mask_fill_polygon(std::vector<uint8_t> &gray, int width, int height, const std::vector<MaskPoint> &poly)
{
	const int n = static_cast<int>(poly.size());
	if (n < 3 || width <= 0 || height <= 0)
		return;

	for (int y = 0; y < height; y++) {
		std::vector<float> xs;
		for (int i = 0; i < n; i++) {
			const MaskPoint a = poly[static_cast<size_t>(i)];
			const MaskPoint b = poly[static_cast<size_t>((i + 1) % n)];
			if ((a.y <= static_cast<float>(y) && b.y > static_cast<float>(y)) ||
			    (b.y <= static_cast<float>(y) && a.y > static_cast<float>(y))) {
				const float t = (static_cast<float>(y) - a.y) / (b.y - a.y);
				xs.push_back(a.x + t * (b.x - a.x));
			}
		}
		if (xs.size() < 2)
			continue;
		std::sort(xs.begin(), xs.end());
		for (size_t i = 0; i + 1 < xs.size(); i += 2) {
			int xa = static_cast<int>(std::ceil(xs[i]));
			int xb = static_cast<int>(std::floor(xs[i + 1]));
			if (xa < 0)
				xa = 0;
			if (xb >= width)
				xb = width - 1;
			for (int x = xa; x <= xb; x++)
				gray[static_cast<size_t>(y) * width + x] = 255;
		}
	}
}

static void mask_feather_inward(std::vector<uint8_t> &gray, int width, int height, int radius)
{
	if (radius <= 0)
		return;
	const int n = width * height;
	std::vector<int> dist(static_cast<size_t>(n), 9999);
	std::vector<int> q;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const int i = y * width + x;
			if (gray[static_cast<size_t>(i)] < 128)
				continue;
			bool border = x == 0 || y == 0 || x == width - 1 || y == height - 1;
			if (!border)
				border = gray[static_cast<size_t>(i - 1)] < 128 || gray[static_cast<size_t>(i + 1)] < 128 ||
					 gray[static_cast<size_t>(i - width)] < 128 ||
					 gray[static_cast<size_t>(i + width)] < 128;
			if (border) {
				dist[static_cast<size_t>(i)] = 0;
				q.push_back(i);
			}
		}
	}
	for (size_t qi = 0; qi < q.size(); qi++) {
		const int i = q[qi];
		const int nd = dist[static_cast<size_t>(i)] + 1;
		if (nd > radius)
			continue;
		const int px = i % width;
		const int py = i / width;
		const int nb[4] = {px > 0 ? i - 1 : -1, px + 1 < width ? i + 1 : -1, py > 0 ? i - width : -1,
				   py + 1 < height ? i + width : -1};
		for (int nbi : nb) {
			if (nbi < 0 || gray[static_cast<size_t>(nbi)] < 128 || dist[static_cast<size_t>(nbi)] <= nd)
				continue;
			dist[static_cast<size_t>(nbi)] = nd;
			q.push_back(nbi);
		}
	}
	for (int i = 0; i < n; i++) {
		if (gray[static_cast<size_t>(i)] < 128)
			continue;
		const int d = dist[static_cast<size_t>(i)];
		if (d >= radius)
			gray[static_cast<size_t>(i)] = 255;
		else
			gray[static_cast<size_t>(i)] = static_cast<uint8_t>(d * 255 / radius);
	}
}

bool mask_snap_edges(const std::vector<uint8_t> &user, const std::vector<uint8_t> &lum, int width, int height, int search,
		     std::vector<uint8_t> &out)
{
	out.clear();
	if (width < 8 || height < 8)
		return false;
	if (static_cast<int>(user.size()) < width * height || static_cast<int>(lum.size()) < width * height)
		return false;
	if (search < 1)
		search = 1;

	const int n = width * height;
	std::vector<uint8_t> painted(static_cast<size_t>(n), 0);
	int count = 0;
	for (int i = 0; i < n; i++) {
		if (user[static_cast<size_t>(i)] >= 40) {
			painted[static_cast<size_t>(i)] = 1;
			count++;
		}
	}
	if (count < 40)
		return false;

	std::vector<uint16_t> mag;
	const int max_mag = mask_sobel(lum, width, height, mag);
	const int thresh = std::max(28, max_mag / 3);

	std::vector<int> label(static_cast<size_t>(n), 0);
	int nlab = 0;
	std::vector<int> stack;
	stack.reserve(1024);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const int start = y * width + x;
			if (!painted[static_cast<size_t>(start)] || label[static_cast<size_t>(start)])
				continue;
			nlab++;
			stack.clear();
			stack.push_back(start);
			label[static_cast<size_t>(start)] = nlab;
			while (!stack.empty()) {
				const int i = stack.back();
				stack.pop_back();
				const int px = i % width;
				const int py = i / width;
				const int nb[4] = {px > 0 ? i - 1 : -1, px + 1 < width ? i + 1 : -1,
						   py > 0 ? i - width : -1, py + 1 < height ? i + width : -1};
				for (int nbi : nb) {
					if (nbi < 0 || !painted[static_cast<size_t>(nbi)] ||
					    label[static_cast<size_t>(nbi)])
						continue;
					label[static_cast<size_t>(nbi)] = nlab;
					stack.push_back(nbi);
				}
			}
		}
	}

	out.assign(static_cast<size_t>(n), 0);
	bool any = false;
	constexpr int k_rays = 160;
	constexpr double k_pi = 3.14159265358979323846;

	for (int lab = 1; lab <= nlab; lab++) {
		int ncc = 0;
		double sx = 0, sy = 0;
		int bx0 = width, by0 = height, bx1 = 0, by1 = 0;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				if (label[static_cast<size_t>(y) * width + x] != lab)
					continue;
				ncc++;
				sx += x;
				sy += y;
				bx0 = std::min(bx0, x);
				by0 = std::min(by0, y);
				bx1 = std::max(bx1, x);
				by1 = std::max(by1, y);
			}
		}
		if (ncc < 40)
			continue;

		const double cx = sx / ncc;
		const double cy = sy / ncc;
		std::vector<MaskPoint> poly;
		poly.reserve(k_rays);
		const int max_t = std::max(8, std::max(bx1 - bx0, by1 - by0));
		for (int i = 0; i < k_rays; i++) {
			const double a = (2.0 * k_pi * i) / k_rays;
			const double dx = std::cos(a);
			const double dy = std::sin(a);
			int exit_t = -1;
			for (int t = 0; t <= max_t + search; t++) {
				const int x = static_cast<int>(std::lround(cx + dx * t));
				const int y = static_cast<int>(std::lround(cy + dy * t));
				if (x < 0 || y < 0 || x >= width || y >= height)
					break;
				if (label[static_cast<size_t>(y) * width + x] == lab)
					exit_t = t;
			}
			if (exit_t < 0)
				continue;
			int best_t = exit_t;
			double best_score = -1;
			for (int t = std::max(0, exit_t - search); t <= exit_t + search; t++) {
				const int x = static_cast<int>(std::lround(cx + dx * t));
				const int y = static_cast<int>(std::lround(cy + dy * t));
				if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1)
					continue;
				const int m = static_cast<int>(mag[static_cast<size_t>(y) * width + x]);
				const double fall = 1.0 - 0.8 * std::abs(t - exit_t) / static_cast<double>(search);
				const double score = m * fall;
				if (score > best_score) {
					best_score = score;
					best_t = t;
				}
			}
			if (best_score < thresh)
				best_t = exit_t;
			poly.push_back({static_cast<float>(cx + dx * best_t), static_cast<float>(cy + dy * best_t)});
		}
		if (poly.size() < 8)
			continue;
		mask_fill_polygon(out, width, height, poly);
		any = true;
	}

	if (!any) {
		out.clear();
		return false;
	}
	mask_feather_inward(out, width, height, 3);
	return true;
}

static void mask_densify_loop(const std::vector<MaskPoint> &in, std::vector<MaskPoint> &out)
{
	out.clear();
	if (in.size() < 2)
		return;
	std::vector<MaskPoint> closed = in;
	const double gap = std::hypot(closed.front().x - closed.back().x, closed.front().y - closed.back().y);
	if (gap > 3.0)
		closed.push_back(closed.front());
	const double spacing = 3.0;
	for (size_t i = 1; i < closed.size(); i++) {
		const float dx = closed[i].x - closed[i - 1].x;
		const float dy = closed[i].y - closed[i - 1].y;
		const double len = std::hypot(dx, dy);
		const int steps = std::max(1, static_cast<int>(len / spacing));
		for (int k = 0; k < steps; k++) {
			const float t = static_cast<float>(k) / static_cast<float>(steps);
			out.push_back({closed[i - 1].x + dx * t, closed[i - 1].y + dy * t});
		}
	}
}

bool mask_magic_shrinkwrap(const std::vector<MaskPoint> &loop, const std::vector<uint8_t> &lum, int width, int height,
			   int search, std::vector<MaskPoint> &out)
{
	out.clear();
	if (width < 8 || height < 8 || loop.size() < 8)
		return false;
	if (static_cast<int>(lum.size()) < width * height)
		return false;
	if (search < 1)
		search = 1;

	std::vector<MaskPoint> dense;
	mask_densify_loop(loop, dense);
	if (dense.size() < 8)
		return false;

	std::vector<uint16_t> mag;
	mask_sobel(lum, width, height, mag);

	auto sample = [&](double x, double y) -> int {
		const int ix = static_cast<int>(std::lround(x));
		const int iy = static_cast<int>(std::lround(y));
		if (ix <= 0 || iy <= 0 || ix >= width - 1 || iy >= height - 1)
			return 0;
		return static_cast<int>(mag[static_cast<size_t>(iy) * width + ix]);
	};

	MaskPoint center{};
	for (const MaskPoint &pt : dense) {
		center.x += pt.x;
		center.y += pt.y;
	}
	center.x /= static_cast<float>(dense.size());
	center.y /= static_cast<float>(dense.size());

	std::vector<int> pull(dense.size(), 0);
	for (size_t i = 0; i < dense.size(); i++) {
		const MaskPoint prev = dense[(i + dense.size() - 1) % dense.size()];
		const MaskPoint next = dense[(i + 1) % dense.size()];
		float tx = next.x - prev.x;
		float ty = next.y - prev.y;
		const double len = std::hypot(tx, ty);
		if (len < 1e-3)
			continue;
		float nx = static_cast<float>(-ty / len);
		float ny = static_cast<float>(tx / len);
		const float to_cx = center.x - dense[i].x;
		const float to_cy = center.y - dense[i].y;
		if (nx * to_cx + ny * to_cy < 0) {
			nx = -nx;
			ny = -ny;
		}

		int ray_max = 1;
		for (int t = 2; t <= search; t++)
			ray_max = std::max(ray_max, sample(dense[i].x + nx * t, dense[i].y + ny * t));
		const int thresh = std::max(28, static_cast<int>(0.50 * ray_max));
		int use_t = 0;
		for (int t = 2; t <= search - 1; t++) {
			const int m = sample(dense[i].x + nx * t, dense[i].y + ny * t);
			if (m < thresh)
				continue;
			const int mo = sample(dense[i].x + nx * (t - 1), dense[i].y + ny * (t - 1));
			const int mi = sample(dense[i].x + nx * (t + 1), dense[i].y + ny * (t + 1));
			if (m >= mo && m >= mi) {
				use_t = t;
				break;
			}
		}
		pull[i] = use_t;
	}

	std::vector<int> smooth = pull;
	for (size_t i = 0; i < dense.size(); i++) {
		int vals[5];
		for (int k = -2; k <= 2; k++)
			vals[k + 2] = pull[static_cast<size_t>((i + static_cast<size_t>(k) + dense.size()) % dense.size())];
		std::sort(vals, vals + 5);
		smooth[i] = vals[2];
	}

	out.reserve(dense.size());
	for (size_t i = 0; i < dense.size(); i++) {
		const MaskPoint prev = dense[(i + dense.size() - 1) % dense.size()];
		const MaskPoint next = dense[(i + 1) % dense.size()];
		float tx = next.x - prev.x;
		float ty = next.y - prev.y;
		const double len = std::hypot(tx, ty);
		float nx = 0, ny = 0;
		if (len >= 1e-3) {
			nx = static_cast<float>(-ty / len);
			ny = static_cast<float>(tx / len);
			const float to_cx = center.x - dense[i].x;
			const float to_cy = center.y - dense[i].y;
			if (nx * to_cx + ny * to_cy < 0) {
				nx = -nx;
				ny = -ny;
			}
		}
		const int t = smooth[i];
		out.push_back({dense[i].x + nx * static_cast<float>(t), dense[i].y + ny * static_cast<float>(t)});
	}
	return out.size() >= 8;
}

void mask_presence_band(const std::vector<uint8_t> &mask, const std::vector<uint8_t> *luma, int width, int height,
			int inset, std::vector<uint8_t> &band)
{
	const int n = width * height;
	band.assign(static_cast<size_t>(n), 0);
	if (width <= 0 || height <= 0 || static_cast<int>(mask.size()) < n)
		return;
	if (inset < 1)
		inset = 1;

	std::vector<uint8_t> inside(static_cast<size_t>(n), 0);
	for (int i = 0; i < n; i++)
		inside[static_cast<size_t>(i)] = mask[static_cast<size_t>(i)] >= 20 ? 1 : 0;

	for (int pass = 0; pass < inset; pass++) {
		std::vector<uint8_t> next = inside;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				const int i = y * width + x;
				if (!inside[static_cast<size_t>(i)])
					continue;
				if (x == 0 || y == 0 || x == width - 1 || y == height - 1 ||
				    !inside[static_cast<size_t>(i - 1)] || !inside[static_cast<size_t>(i + 1)] ||
				    !inside[static_cast<size_t>(i - width)] || !inside[static_cast<size_t>(i + width)])
					next[static_cast<size_t>(i)] = 0;
			}
		}
		inside.swap(next);
	}

	int keep = 0;
	for (int i = 0; i < n; i++) {
		if (inside[static_cast<size_t>(i)]) {
			band[static_cast<size_t>(i)] = 255;
			keep++;
		}
	}
	if (keep < 16) {
		/* Tiny mask: use the paint as-is. */
		for (int i = 0; i < n; i++)
			band[static_cast<size_t>(i)] = mask[static_cast<size_t>(i)] >= 20 ? 255 : 0;
		return;
	}

	if (!luma || static_cast<int>(luma->size()) < n)
		return;

	std::vector<uint16_t> mag;
	mask_sobel(*luma, width, height, mag);
	int structured = 0;
	std::vector<uint8_t> gated(static_cast<size_t>(n), 0);
	const int mag_min = 28;
	for (int i = 0; i < n; i++) {
		if (band[static_cast<size_t>(i)] < 128)
			continue;
		if (mag[static_cast<size_t>(i)] >= mag_min) {
			gated[static_cast<size_t>(i)] = 255;
			structured++;
		}
	}
	if (structured >= 24)
		band.swap(gated);
}

void mask_resize_luma(const std::vector<uint8_t> &src, int src_w, int src_h, std::vector<uint8_t> &dst, int dst_w,
		      int dst_h)
{
	dst.assign(static_cast<size_t>(std::max(0, dst_w)) * std::max(0, dst_h), 0);
	if (src_w < 1 || src_h < 1 || dst_w < 1 || dst_h < 1)
		return;
	if (static_cast<int>(src.size()) < src_w * src_h)
		return;

	for (int y = 0; y < dst_h; y++) {
		const int sy = std::min(src_h - 1, y * src_h / dst_h);
		for (int x = 0; x < dst_w; x++) {
			const int sx = std::min(src_w - 1, x * src_w / dst_w);
			dst[static_cast<size_t>(y) * dst_w + x] = src[static_cast<size_t>(sy) * src_w + sx];
		}
	}
}

void mask_resize_mask(const std::vector<uint8_t> &src, int src_w, int src_h, std::vector<uint8_t> &dst, int dst_w,
		      int dst_h)
{
	mask_resize_luma(src, src_w, src_h, dst, dst_w, dst_h);
}

bool mask_presence_score(const std::vector<uint8_t> &ref_luma, const std::vector<uint8_t> &cur_luma,
			 const std::vector<uint8_t> &band, int width, int height, float *score)
{
	if (!score)
		return false;
	*score = 0;
	const int n = width * height;
	if (width < 3 || height < 3)
		return false;
	if (static_cast<int>(ref_luma.size()) < n || static_cast<int>(cur_luma.size()) < n ||
	    static_cast<int>(band.size()) < n)
		return false;

	std::vector<uint16_t> mag_r;
	std::vector<uint16_t> mag_c;
	mask_sobel(ref_luma, width, height, mag_r);
	mask_sobel(cur_luma, width, height, mag_c);

	double sum_r = 0, sum_c = 0;
	int count = 0;
	for (int i = 0; i < n; i++) {
		if (band[static_cast<size_t>(i)] < 128)
			continue;
		sum_r += mag_r[static_cast<size_t>(i)];
		sum_c += mag_c[static_cast<size_t>(i)];
		count++;
	}
	if (count < 16)
		return false;

	const double mean_r = sum_r / count;
	const double mean_c = sum_c / count;
	double num = 0, den_r = 0, den_c = 0;
	for (int i = 0; i < n; i++) {
		if (band[static_cast<size_t>(i)] < 128)
			continue;
		const double dr = mag_r[static_cast<size_t>(i)] - mean_r;
		const double dc = mag_c[static_cast<size_t>(i)] - mean_c;
		num += dr * dc;
		den_r += dr * dr;
		den_c += dc * dc;
	}
	if (den_r < 1e-6 && den_c < 1e-6) {
		*score = 1.0f;
		return true;
	}
	if (den_r < 1e-6 || den_c < 1e-6) {
		*score = 0.0f;
		return true;
	}
	const double r = num / std::sqrt(den_r * den_c);
	*score = r < 0 ? 0.0f : static_cast<float>(r > 1 ? 1 : r);
	return true;
}

float mask_presence_threshold(int match_percent)
{
	if (match_percent < 0)
		match_percent = 0;
	if (match_percent > 100)
		match_percent = 100;
	/* 0 → 0.22 (lenient), 50 → 0.50, 100 → 0.78 (inventory-like UI should not pass). */
	return 0.22f + 0.56f * (static_cast<float>(match_percent) / 100.0f);
}

void mask_blob_outline(const std::vector<uint8_t> &mask, int width, int height, std::vector<uint8_t> &outline)
{
	const int n = width * height;
	outline.assign(static_cast<size_t>(n), 0);
	if (width < 2 || height < 2 || static_cast<int>(mask.size()) < n)
		return;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const int i = y * width + x;
			if (mask[static_cast<size_t>(i)] < 20)
				continue;
			const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1 ||
					  mask[static_cast<size_t>(i - 1)] < 20 || mask[static_cast<size_t>(i + 1)] < 20 ||
					  mask[static_cast<size_t>(i - width)] < 20 ||
					  mask[static_cast<size_t>(i + width)] < 20;
			if (edge)
				outline[static_cast<size_t>(i)] = 255;
		}
	}
}

static void mask_dilate1(std::vector<uint8_t> &img, int width, int height)
{
	const int n = width * height;
	std::vector<uint8_t> src = img;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (src[static_cast<size_t>(y) * width + x] < 128)
				continue;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					const int nx = x + dx;
					const int ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= width || ny >= height)
						continue;
					img[static_cast<size_t>(ny) * width + nx] = 255;
				}
			}
		}
	}
	(void)n;
}

static void mask_scale_about_centroid(const std::vector<uint8_t> &src, int width, int height, float scale,
				      std::vector<uint8_t> &out)
{
	const int n = width * height;
	out.assign(static_cast<size_t>(n), 0);
	if (scale <= 0.01f)
		return;
	double sx = 0, sy = 0;
	int count = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (src[static_cast<size_t>(y) * width + x] < 128)
				continue;
			sx += x;
			sy += y;
			count++;
		}
	}
	if (count < 4)
		return;
	const double cx = sx / count;
	const double cy = sy / count;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (src[static_cast<size_t>(y) * width + x] < 128)
				continue;
			const int nx = static_cast<int>(std::lround(cx + (x - cx) * scale));
			const int ny = static_cast<int>(std::lround(cy + (y - cy) * scale));
			if (nx < 0 || ny < 0 || nx >= width || ny >= height)
				continue;
			out[static_cast<size_t>(ny) * width + nx] = 255;
		}
	}
	mask_dilate1(out, width, height);
}

static float mask_outline_hit_rate(const std::vector<uint8_t> &outline, const std::vector<uint16_t> &mag,
				   const std::vector<uint8_t> *stability, int width, int height, int search, int mag_min)
{
	int hits = 0, n = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (outline[static_cast<size_t>(y) * width + x] < 128)
				continue;
			n++;
			int best = 0;
			for (int dy = -search; dy <= search; dy++) {
				for (int dx = -search; dx <= search; dx++) {
					const int nx = x + dx;
					const int ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= width || ny >= height)
						continue;
					int m = mag[static_cast<size_t>(ny) * width + nx];
					if (stability)
						m = static_cast<int>(m * (*stability)[static_cast<size_t>(ny) * width + nx] /
								     255.0);
					if (m > best)
						best = m;
				}
			}
			if (best >= mag_min)
				hits++;
		}
	}
	if (n < 8)
		return 0;
	return static_cast<float>(hits) / static_cast<float>(n);
}

bool mask_presence_outline_score(const std::vector<uint8_t> &mask, const std::vector<uint8_t> &cur_luma,
				 const std::vector<uint8_t> *prev_luma, int width, int height, float *score)
{
	if (!score)
		return false;
	*score = 0;
	if (width < 8 || height < 8)
		return false;
	const int n = width * height;
	if (static_cast<int>(mask.size()) < n || static_cast<int>(cur_luma.size()) < n)
		return false;

	std::vector<uint8_t> outline;
	mask_blob_outline(mask, width, height, outline);
	mask_dilate1(outline, width, height);

	std::vector<uint16_t> mag;
	mask_sobel(cur_luma, width, height, mag);

	std::vector<uint8_t> stability;
	const std::vector<uint8_t> *stab_ptr = nullptr;
	if (prev_luma && static_cast<int>(prev_luma->size()) >= n) {
		stability.resize(static_cast<size_t>(n));
		for (int i = 0; i < n; i++) {
			const int d = std::abs(static_cast<int>(cur_luma[static_cast<size_t>(i)]) -
					       static_cast<int>((*prev_luma)[static_cast<size_t>(i)]));
			int s = 255 - std::min(255, d * 6);
			if (s < 0)
				s = 0;
			stability[static_cast<size_t>(i)] = static_cast<uint8_t>(s);
		}
		stab_ptr = &stability;
	}

	std::vector<int> mags;
	mags.reserve(static_cast<size_t>(n));
	for (int i = 0; i < n; i++)
		mags.push_back(mag[static_cast<size_t>(i)]);
	std::nth_element(mags.begin(), mags.begin() + mags.size() / 2, mags.end());
	const int median_mag = mags[mags.size() / 2];
	const int mag_min = std::max(20, median_mag + median_mag / 2);

	const int search = std::max(2, std::min(width, height) / 24);
	const float scales[] = {1.0f, 0.88f, 0.76f, 0.64f};
	float best = 0;
	for (float sc : scales) {
		std::vector<uint8_t> scaled;
		if (sc > 0.99f)
			scaled = outline;
		else
			mask_scale_about_centroid(outline, width, height, sc, scaled);
		const float hit = mask_outline_hit_rate(scaled, mag, stab_ptr, width, height, search, mag_min);
		if (hit > best)
			best = hit;
	}
	*score = best;
	return true;
}

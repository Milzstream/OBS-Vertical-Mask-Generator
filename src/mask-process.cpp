#include "mask-process.hpp"

#include <algorithm>
#include <array>
#include <climits>
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

MaskScale mask_scale_cutout(int live_w, int live_h, int crop_left, int crop_top, int crop_right, int crop_bottom,
			    int mask_w, int mask_h)
{
	MaskScale s;
	if (live_w <= 0 || live_h <= 0)
		return s;

	const int left = std::max(crop_left, 0);
	const int top = std::max(crop_top, 0);
	const int right = std::max(crop_right, 0);
	const int bottom = std::max(crop_bottom, 0);

	auto unscaled = [&]() {
		const int w = live_w - left - right;
		const int h = live_h - top - bottom;
		if (w <= 0 || h <= 0)
			return s;
		s.left = left;
		s.top = top;
		s.width = w;
		s.height = h;
		s.empty = false;
		return s;
	};

	if (mask_w <= 0 || mask_h <= 0)
		return unscaled();

	const int auth_w = left + mask_w + right;
	const int auth_h = top + mask_h + bottom;
	if (auth_w <= 0 || auth_h <= 0)
		return s;
	if (auth_w == live_w && auth_h == live_h)
		return unscaled();

	const int sl = static_cast<int>(std::lround(static_cast<double>(left) * live_w / auth_w));
	const int sr = static_cast<int>(std::lround(static_cast<double>(right) * live_w / auth_w));
	const int st = static_cast<int>(std::lround(static_cast<double>(top) * live_h / auth_h));
	const int sb = static_cast<int>(std::lround(static_cast<double>(bottom) * live_h / auth_h));
	const int w = live_w - sl - sr;
	const int h = live_h - st - sb;
	if (w <= 0 || h <= 0)
		return s;
	s.left = sl;
	s.top = st;
	s.width = w;
	s.height = h;
	s.empty = false;
	return s;
}

void mask_fill_polygon(std::vector<uint8_t> &gray, int width, int height, const std::vector<MaskPoint> &poly)
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
	mask_sobel(lum, width, height, mag);

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
	constexpr int k_rays = 192;
	constexpr double k_pi = 3.14159265358979323846;

	auto mag_at = [&](double cx, double cy, double dx, double dy, int t) -> int {
		const int x = static_cast<int>(std::lround(cx + dx * t));
		const int y = static_cast<int>(std::lround(cy + dy * t));
		if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1)
			return 0;
		return static_cast<int>(mag[static_cast<size_t>(y) * width + x]);
	};

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

			const int t0 = std::max(0, exit_t - search);
			const int t1 = exit_t + search;
			int local_max = 1;
			for (int t = t0; t <= t1; t++)
				local_max = std::max(local_max, mag_at(cx, cy, dx, dy, t));
			const int local_thresh = std::max(24, local_max / 4);

			int best_t = exit_t;
			int best_dist = search + 1;
			for (int t = t0; t <= t1; t++) {
				const int m = mag_at(cx, cy, dx, dy, t);
				if (m < local_thresh)
					continue;
				const int ml = mag_at(cx, cy, dx, dy, t - 1);
				const int mr = mag_at(cx, cy, dx, dy, t + 1);
				if (m < ml || m < mr)
					continue;
				const int dist = std::abs(t - exit_t);
				if (dist < best_dist) {
					best_dist = dist;
					best_t = t;
				}
			}
			poly.push_back({static_cast<float>(cx + dx * best_t), static_cast<float>(cy + dy * best_t)});
		}
		if (poly.size() < 8)
			continue;

		std::vector<MaskPoint> smooth = poly;
		const int pn = static_cast<int>(poly.size());
		const int wr = 3;
		for (int i = 0; i < pn; i++) {
			float px = 0, py = 0;
			for (int k = -wr; k <= wr; k++) {
				const MaskPoint &p = poly[static_cast<size_t>((i + k + pn * 8) % pn)];
				px += p.x;
				py += p.y;
			}
			smooth[static_cast<size_t>(i)] = {px / static_cast<float>(wr * 2 + 1),
							  py / static_cast<float>(wr * 2 + 1)};
		}
		mask_fill_polygon(out, width, height, smooth);
		any = true;
	}

	if (!any) {
		out.clear();
		return false;
	}
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

void mask_presence_prepare_mask(std::vector<uint8_t> &gray, int width, int height, int expand, int feather)
{
	if (expand == 0 && feather <= 0)
		return;
	mask_binarize(gray);
	if (expand != 0)
		mask_expand(gray, width, height, expand);
	if (feather > 0)
		mask_feather(gray, width, height, feather);
}

static bool presence_pearson(const std::vector<uint16_t> &mag_r, const std::vector<uint16_t> &mag_c,
			     const std::vector<uint8_t> &band, int width, int height, int dx, int dy, float *score)
{
	if (!score)
		return false;
	*score = 0;
	const int n = width * height;
	if (width < 3 || height < 3)
		return false;
	if (static_cast<int>(mag_r.size()) < n || static_cast<int>(mag_c.size()) < n ||
	    static_cast<int>(band.size()) < n)
		return false;

	double sum_r = 0, sum_c = 0;
	int count = 0;
	for (int y = 0; y < height; y++) {
		const int cy = y + dy;
		if (cy < 0 || cy >= height)
			continue;
		for (int x = 0; x < width; x++) {
			if (band[static_cast<size_t>(y) * width + x] < 128)
				continue;
			const int cx = x + dx;
			if (cx < 0 || cx >= width)
				continue;
			sum_r += mag_r[static_cast<size_t>(y) * width + x];
			sum_c += mag_c[static_cast<size_t>(cy) * width + cx];
			count++;
		}
	}
	if (count < 16)
		return false;

	const double mean_r = sum_r / count;
	const double mean_c = sum_c / count;
	double num = 0, den_r = 0, den_c = 0;
	for (int y = 0; y < height; y++) {
		const int cy = y + dy;
		if (cy < 0 || cy >= height)
			continue;
		for (int x = 0; x < width; x++) {
			if (band[static_cast<size_t>(y) * width + x] < 128)
				continue;
			const int cx = x + dx;
			if (cx < 0 || cx >= width)
				continue;
			const double dr = mag_r[static_cast<size_t>(y) * width + x] - mean_r;
			const double dc = mag_c[static_cast<size_t>(cy) * width + cx] - mean_c;
			num += dr * dc;
			den_r += dr * dr;
			den_c += dc * dc;
		}
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
	return presence_pearson(mag_r, mag_c, band, width, height, 0, 0, score);
}

std::string mask_presence_next_target(const std::vector<std::string> &sorted_unique, const std::string &last)
{
	if (sorted_unique.empty())
		return {};
	if (last.empty())
		return sorted_unique.front();
	for (size_t i = 0; i < sorted_unique.size(); i++) {
		if (sorted_unique[i] != last)
			continue;
		return sorted_unique[(i + 1) % sorted_unique.size()];
	}
	return sorted_unique.front();
}

float mask_presence_threshold(int match_percent)
{
	if (match_percent < 0)
		match_percent = 0;
	if (match_percent > 100)
		match_percent = 100;
	return static_cast<float>(match_percent) / 100.0f;
}

bool mask_presence_gate(MaskPresenceGate &gate, float score, int match_percent, int hysteresis)
{
	if (hysteresis < 1)
		hysteresis = 1;
	const float thresh = mask_presence_threshold(match_percent);
	const float show_need = std::min(0.95f, thresh + 0.04f);
	const float hide_need = std::max(0.0f, thresh - 0.04f);
	bool want = gate.shown;
	if (gate.shown) {
		if (score < hide_need)
			want = false;
	} else if (score >= show_need) {
		want = true;
	}
	if (want == gate.shown) {
		gate.streak = 0;
		return false;
	}
	gate.streak++;
	if (gate.streak >= hysteresis) {
		gate.shown = want;
		gate.streak = 0;
		return true;
	}
	return false;
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

void mask_presence_rim(const std::vector<uint8_t> &mask, int width, int height, int thickness, std::vector<uint8_t> &rim)
{
	mask_blob_outline(mask, width, height, rim);
	if (thickness < 1)
		thickness = 1;
	for (int i = 0; i < thickness; i++)
		mask_dilate1(rim, width, height);
	const int n = width * height;
	if (static_cast<int>(mask.size()) < n)
		return;
	for (int i = 0; i < n; i++) {
		if (mask[static_cast<size_t>(i)] < 20)
			rim[static_cast<size_t>(i)] = 0;
	}
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

static void mask_scale_planes_about_centroid(const std::vector<uint8_t> &luma, const std::vector<uint8_t> &band,
					     int width, int height, float scale, std::vector<uint8_t> &out_luma,
					     std::vector<uint8_t> &out_band)
{
	const int n = width * height;
	out_luma.assign(static_cast<size_t>(n), 0);
	out_band.assign(static_cast<size_t>(n), 0);
	if (scale <= 0.01f || static_cast<int>(luma.size()) < n || static_cast<int>(band.size()) < n)
		return;
	double sx = 0, sy = 0;
	int count = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (band[static_cast<size_t>(y) * width + x] < 128)
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
			if (band[static_cast<size_t>(y) * width + x] < 128)
				continue;
			const int nx = static_cast<int>(std::lround(cx + (x - cx) * scale));
			const int ny = static_cast<int>(std::lround(cy + (y - cy) * scale));
			if (nx < 0 || ny < 0 || nx >= width || ny >= height)
				continue;
			out_luma[static_cast<size_t>(ny) * width + nx] = luma[static_cast<size_t>(y) * width + x];
			out_band[static_cast<size_t>(ny) * width + nx] = 255;
		}
	}
	mask_dilate1(out_band, width, height);
}

bool mask_presence_match(const std::vector<uint8_t> &ref_luma, const std::vector<uint8_t> &cur_luma,
			 const std::vector<uint8_t> &band, int width, int height, int max_shift, float *score)
{
	if (!score)
		return false;
	*score = 0;
	const int n = width * height;
	if (width < 8 || height < 8)
		return false;
	if (static_cast<int>(ref_luma.size()) < n || static_cast<int>(cur_luma.size()) < n ||
	    static_cast<int>(band.size()) < n)
		return false;
	if (max_shift < 0)
		max_shift = 0;
	if (max_shift > 8)
		max_shift = 8;

	std::vector<uint16_t> mag_c;
	mask_sobel(cur_luma, width, height, mag_c);

	const float scales[] = {1.0f, 0.88f, 0.76f, 0.64f};
	float best = 0;
	bool any = false;
	for (float sc : scales) {
		const std::vector<uint8_t> *ref_p = &ref_luma;
		const std::vector<uint8_t> *band_p = &band;
		std::vector<uint8_t> ref_s;
		std::vector<uint8_t> band_s;
		if (sc <= 0.99f) {
			mask_scale_planes_about_centroid(ref_luma, band, width, height, sc, ref_s, band_s);
			ref_p = &ref_s;
			band_p = &band_s;
		}
		std::vector<uint16_t> mag_r;
		mask_sobel(*ref_p, width, height, mag_r);
		const int shift = sc > 0.99f ? max_shift : std::min(max_shift, 2);
		for (int dy = -shift; dy <= shift; dy++) {
			for (int dx = -shift; dx <= shift; dx++) {
				float s = 0;
				if (!presence_pearson(mag_r, mag_c, *band_p, width, height, dx, dy, &s))
					continue;
				any = true;
				if (s > best)
					best = s;
			}
		}
	}
	*score = best;
	return any;
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

MaskLoopBounds mask_loop_bounds(const std::vector<MaskPoint> &loop, int frame_w, int frame_h, int pad)
{
	MaskLoopBounds b;
	if (loop.size() < 3 || frame_w < 1 || frame_h < 1)
		return b;
	if (pad < 0)
		pad = 0;
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
	int x0 = static_cast<int>(std::floor(min_x)) - pad;
	int y0 = static_cast<int>(std::floor(min_y)) - pad;
	int x1 = static_cast<int>(std::ceil(max_x)) + pad;
	int y1 = static_cast<int>(std::ceil(max_y)) + pad;
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > frame_w - 1)
		x1 = frame_w - 1;
	if (y1 > frame_h - 1)
		y1 = frame_h - 1;
	if (x1 < x0 || y1 < y0)
		return b;
	b.x = x0;
	b.y = y0;
	b.width = x1 - x0 + 1;
	b.height = y1 - y0 + 1;
	b.empty = b.width < 1 || b.height < 1;
	return b;
}

void mask_sam_box_to_input(float x0, float y0, float x1, float y1, int crop_w, int crop_h, float out_xyxy[4])
{
	const float longest = static_cast<float>(std::max(crop_w, crop_h));
	const float scale = longest > 0.f ? 1024.f / longest : 1.f;
	out_xyxy[0] = x0 * scale;
	out_xyxy[1] = y0 * scale;
	out_xyxy[2] = x1 * scale;
	out_xyxy[3] = y1 * scale;
}

bool mask_point_in_loop(float x, float y, const std::vector<MaskPoint> &loop)
{
	if (loop.size() < 3)
		return false;
	bool inside = false;
	for (size_t i = 0, j = loop.size() - 1; i < loop.size(); j = i++) {
		const float yi = loop[i].y;
		const float yj = loop[j].y;
		const float xi = loop[i].x;
		const float xj = loop[j].x;
		if ((yi > y) == (yj > y))
			continue;
		const float dy = yj - yi;
		if (dy == 0.f)
			continue;
		if (x < (xj - xi) * (y - yi) / dy + xi)
			inside = !inside;
	}
	return inside;
}

void mask_clip_to_loop(std::vector<uint8_t> &gray, int width, int height, const std::vector<MaskPoint> &loop)
{
	if (width < 1 || height < 1 || static_cast<int>(gray.size()) < width * height)
		return;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const size_t i = static_cast<size_t>(y) * width + x;
			if (gray[i] < 128)
				continue;
			if (!mask_point_in_loop(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, loop))
				gray[i] = 0;
		}
	}
}

bool mask_magic_pick_ok(int mask_pixels, int crop_pixels, float iou)
{
	if (crop_pixels <= 0 || mask_pixels < 16)
		return false;
	if (!(iou >= 0.45f))
		return false;
	if (mask_pixels > static_cast<int>(0.97f * static_cast<float>(crop_pixels)))
		return false;
	if (mask_pixels * 1000 < crop_pixels * 4)
		return false;
	return true;
}

/* ---- GrabCut pick (Rother, Kolmogorov & Blake, SIGGRAPH 2004) ------------
 * Clean-room implementation of the published algorithm: trimap, 5-component
 * full-covariance GMMs for foreground and background, and iterated graph
 * cuts with a contrast-sensitive smoothness term. No ONNX, no learned model.
 * The graph cut is solved with a highest-label push/relabel max-flow
 * (Goldberg & Tarjan) written for this file. */

namespace {

struct GcComp {
	double weight = 0;
	double mean[3] = {0, 0, 0};
	double inv[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
	double logdet = 0;

	double neglog(const float z[3]) const
	{
		const double d[3] = {z[0] - mean[0], z[1] - mean[1], z[2] - mean[2]};
		const double q = d[0] * (inv[0] * d[0] + inv[1] * d[1] + inv[2] * d[2]) +
				 d[1] * (inv[3] * d[0] + inv[4] * d[1] + inv[5] * d[2]) +
				 d[2] * (inv[6] * d[0] + inv[7] * d[1] + inv[8] * d[2]);
		return -std::log(weight) + 0.5 * (logdet + q + 3.0 * 1.8378770664093453);
	}
};

struct GcGmm {
	std::vector<GcComp> comps;

	bool empty() const { return comps.empty(); }

	double neglog(const float z[3]) const
	{
		double best = 1e100;
		for (const GcComp &c : comps)
			best = std::min(best, c.neglog(z));
		return best;
	}
};

/* Fit a GMM with up to kmax components: deterministic farthest-point seeding,
 * Lloyd (k-means) iterations on a subsample, then full-covariance component
 * statistics over every sample, with a ridge on the covariance diagonal that
 * is raised until the matrix is invertible. */
GcGmm gc_fit_gmm(const std::vector<std::array<float, 3>> &samples, int kmax)
{
	GcGmm gmm;
	if (samples.empty() || kmax < 1)
		return gmm;

	/* Subsample for the k-means stage only. */
	std::vector<std::array<float, 3>> seed = samples;
	if (seed.size() > 4000) {
		std::vector<std::array<float, 3>> sub;
		const double stride = static_cast<double>(seed.size()) / 4000.0;
		for (double i = 0; i < static_cast<double>(seed.size()); i += stride)
			sub.push_back(seed[static_cast<size_t>(i)]);
		seed.swap(sub);
	}
	int k = kmax;
	if (static_cast<int>(seed.size()) < k)
		k = static_cast<int>(seed.size());
	if (k < 1)
		k = 1;

	/* Farthest-point seeding (deterministic). */
	double mean[3] = {0, 0, 0};
	for (const auto &s : seed)
		for (int c = 0; c < 3; c++)
			mean[c] += s[c];
	for (int c = 0; c < 3; c++)
		mean[c] /= static_cast<double>(seed.size());
	std::vector<std::array<double, 3>> centres;
	{
		size_t best_i = 0;
		double best_d = -1;
		for (size_t i = 0; i < seed.size(); i++) {
			double d = 0;
			for (int c = 0; c < 3; c++) {
				const double diff = seed[i][c] - mean[c];
				d += diff * diff;
			}
			if (d > best_d) {
				best_d = d;
				best_i = i;
			}
		}
		(void)best_d;
		centres.push_back({seed[best_i][0], seed[best_i][1], seed[best_i][2]});
	}
	while (static_cast<int>(centres.size()) < k) {
		size_t best_i = 0;
		double best_d = -1;
		for (size_t i = 0; i < seed.size(); i++) {
			double mind = 1e100;
			for (const auto &cen : centres) {
				double d = 0;
				for (int c = 0; c < 3; c++) {
					const double diff = seed[i][c] - cen[c];
					d += diff * diff;
				}
				mind = std::min(mind, d);
			}
			if (mind > best_d) {
				best_d = mind;
				best_i = i;
			}
		}
		if (best_d <= 0)
			break;
		centres.push_back({seed[best_i][0], seed[best_i][1], seed[best_i][2]});
	}
	k = static_cast<int>(centres.size());

	auto nearest = [&centres](const std::array<float, 3> &s) {
		int best = 0;
		double best_d = 1e100;
		for (int j = 0; j < static_cast<int>(centres.size()); j++) {
			double d = 0;
			for (int c = 0; c < 3; c++) {
				const double diff = s[c] - centres[j][c];
				d += diff * diff;
			}
			if (d < best_d) {
				best_d = d;
				best = j;
			}
		}
		return best;
	};

	/* Lloyd iterations on the seed subsample. */
	for (int it = 0; it < 25; it++) {
		std::vector<std::array<double, 3>> sum(k, {0, 0, 0});
		std::vector<int> cnt(k, 0);
		for (const auto &s : seed) {
			const int j = nearest(s);
			cnt[j]++;
			for (int c = 0; c < 3; c++)
				sum[j][c] += s[c];
		}
		bool moved = false;
		for (int j = 0; j < k; j++) {
			if (!cnt[j])
				continue;
			for (int c = 0; c < 3; c++) {
				const double v = sum[j][c] / cnt[j];
				if (std::fabs(v - centres[j][c]) > 0.01)
					moved = true;
				centres[j][c] = v;
			}
		}
		if (!moved)
			break;
	}

	/* Component statistics over all samples. */
	struct Acc {
		double n = 0;
		double s[3] = {0, 0, 0};
		double ss[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
	};
	std::vector<Acc> acc(k);
	for (const auto &s : samples) {
		const int j = nearest(s);
		Acc &a = acc[j];
		a.n += 1;
		for (int r = 0; r < 3; r++) {
			a.s[r] += s[r];
			for (int c = 0; c < 3; c++)
				a.ss[r * 3 + c] += static_cast<double>(s[r]) * s[c];
		}
	}
	const double total = static_cast<double>(samples.size());
	for (int j = 0; j < k; j++) {
		if (acc[j].n < 2)
			continue;
		GcComp comp;
		comp.weight = acc[j].n / total;
		double cov[9];
		for (int c = 0; c < 3; c++)
			comp.mean[c] = acc[j].s[c] / acc[j].n;
		for (int r = 0; r < 3; r++) {
			for (int c = 0; c < 3; c++)
				cov[r * 3 + c] = acc[j].ss[r * 3 + c] / acc[j].n - comp.mean[r] * comp.mean[c];
		}
		bool ok = false;
		for (double ridge = 16.0; ridge <= 4096.0 && !ok; ridge *= 4.0) {
			double m[9];
			for (int i = 0; i < 9; i++)
				m[i] = cov[i];
			m[0] += ridge;
			m[4] += ridge;
			m[8] += ridge;
			const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
					   m[2] * (m[3] * m[7] - m[4] * m[6]);
			if (!(det > 1.0) || !std::isfinite(det))
				continue;
			comp.inv[0] = (m[4] * m[8] - m[5] * m[7]) / det;
			comp.inv[1] = (m[2] * m[7] - m[1] * m[8]) / det;
			comp.inv[2] = (m[1] * m[5] - m[2] * m[4]) / det;
			comp.inv[3] = (m[5] * m[6] - m[3] * m[8]) / det;
			comp.inv[4] = (m[0] * m[8] - m[2] * m[6]) / det;
			comp.inv[5] = (m[2] * m[3] - m[0] * m[5]) / det;
			comp.inv[6] = (m[3] * m[7] - m[4] * m[6]) / det;
			comp.inv[7] = (m[1] * m[6] - m[0] * m[7]) / det;
			comp.inv[8] = (m[0] * m[4] - m[1] * m[3]) / det;
			comp.logdet = std::log(det);
			ok = true;
		}
		if (ok)
			gmm.comps.push_back(comp);
	}
	return gmm;
}

/* Highest-label push/relabel max-flow. Nodes reachable from the source in
 * the residual graph after the flow form the source side of the min cut. */
struct GcGraph {
	struct Edge {
		int to;
		int rev;
		double cap;
	};

	int n;
	std::vector<std::vector<Edge>> g;
	std::vector<double> excess;
	std::vector<int> height;
	std::vector<int> cur;
	/* Nodes the gap heuristic must never retire (hard constraints). */
	std::vector<char> keep;

	GcGraph() = delete;

	explicit GcGraph(int nodes) : n(nodes), g(static_cast<size_t>(nodes)), excess(static_cast<size_t>(nodes), 0),
				      height(static_cast<size_t>(nodes), 0), cur(static_cast<size_t>(nodes), 0),
				      keep(static_cast<size_t>(nodes), 0)
	{
	}

	void add_directed(int u, int v, double cap)
	{
		Edge a{v, static_cast<int>(g[static_cast<size_t>(v)].size()), cap};
		Edge b{u, static_cast<int>(g[static_cast<size_t>(u)].size()), 0};
		g[static_cast<size_t>(u)].push_back(a);
		g[static_cast<size_t>(v)].push_back(b);
	}

	/* An n-link: crossing it in either direction costs cap. */
	void add_cut_edge(int u, int v, double cap)
	{
		Edge a{v, static_cast<int>(g[static_cast<size_t>(v)].size()), cap};
		Edge b{u, static_cast<int>(g[static_cast<size_t>(u)].size()), cap};
		g[static_cast<size_t>(u)].push_back(a);
		g[static_cast<size_t>(v)].push_back(b);
	}

	std::vector<char> mincut_source_side(int s, int t)
	{
		const double eps = 1e-9;
		const int big = n + 5;
		height[static_cast<size_t>(s)] = n;
		std::vector<std::vector<int>> buckets(static_cast<size_t>(4 * n + 16));
		std::vector<char> inq(static_cast<size_t>(n), 0);
		/* Gap heuristic: cnt[h] counts nodes at height h. When a height
		 * level empties, no residual arc crosses it, so every node
		 * strictly between it and n can never reach the sink again:
		 * those nodes are on the source side of the min cut and are
		 * retired (forced to the source side below). */
		std::vector<int> cnt(static_cast<size_t>(4 * n + 16), 0);
		std::vector<char> dead(static_cast<size_t>(n), 0);
		for (int v = 0; v < n; v++)
			cnt[static_cast<size_t>(height[static_cast<size_t>(v)])]++;
		int top = 0;
		auto enqueue = [&](int v) {
			if (v == s || v == t || dead[static_cast<size_t>(v)] || excess[static_cast<size_t>(v)] <= eps ||
			    inq[static_cast<size_t>(v)])
				return;
			const int hv = height[static_cast<size_t>(v)];
			if (hv >= static_cast<int>(buckets.size()))
				buckets.resize(static_cast<size_t>(hv + 64));
			inq[static_cast<size_t>(v)] = 1;
			buckets[static_cast<size_t>(hv)].push_back(v);
			top = std::max(top, hv);
		};

		for (Edge &e : g[static_cast<size_t>(s)]) {
			if (e.cap <= eps || e.to == s)
				continue;
			const double f = e.cap;
			e.cap = 0;
			g[static_cast<size_t>(e.to)][static_cast<size_t>(e.rev)].cap += f;
			excess[static_cast<size_t>(e.to)] += f;
			excess[static_cast<size_t>(s)] -= f;
			enqueue(e.to);
		}

		while (top >= 0) {
			if (buckets[static_cast<size_t>(top)].empty()) {
				top--;
				continue;
			}
			const int v = buckets[static_cast<size_t>(top)].back();
			buckets[static_cast<size_t>(top)].pop_back();
			inq[static_cast<size_t>(v)] = 0;
			if (dead[static_cast<size_t>(v)])
				continue;
			while (excess[static_cast<size_t>(v)] > eps && !dead[static_cast<size_t>(v)]) {
				if (cur[static_cast<size_t>(v)] < static_cast<int>(g[static_cast<size_t>(v)].size())) {
					Edge &e = g[static_cast<size_t>(v)][static_cast<size_t>(cur[static_cast<size_t>(v)])];
					if (e.cap > eps && height[static_cast<size_t>(v)] == height[static_cast<size_t>(e.to)] + 1) {
						const double f = std::min(excess[static_cast<size_t>(v)], e.cap);
						e.cap -= f;
						g[static_cast<size_t>(e.to)][static_cast<size_t>(e.rev)].cap += f;
						excess[static_cast<size_t>(v)] -= f;
						excess[static_cast<size_t>(e.to)] += f;
						enqueue(e.to);
					} else {
						cur[static_cast<size_t>(v)]++;
					}
				} else {
					int minh = INT_MAX;
					for (const Edge &e : g[static_cast<size_t>(v)]) {
						if (e.cap > eps)
							minh = std::min(minh, height[static_cast<size_t>(e.to)]);
					}
					if (minh == INT_MAX)
						break;
					const int oldh = height[static_cast<size_t>(v)];
					int newh = minh + 1;
					if (newh > n && !keep[static_cast<size_t>(v)]) {
						newh = big;
						dead[static_cast<size_t>(v)] = 1;
					}
					cnt[static_cast<size_t>(oldh)]--;
					if (newh >= static_cast<int>(cnt.size()))
						cnt.resize(static_cast<size_t>(newh + 64), 0);
					cnt[static_cast<size_t>(newh)]++;
					height[static_cast<size_t>(v)] = newh;
					cur[static_cast<size_t>(v)] = 0;
					top = std::max(top, newh);
					if (cnt[static_cast<size_t>(oldh)] == 0 && oldh > 0 && oldh < n) {
						for (int u = 0; u < n; u++) {
							const int hu = height[static_cast<size_t>(u)];
							if (u == s || u == t || keep[static_cast<size_t>(u)] ||
							    dead[static_cast<size_t>(u)] || hu <= oldh || hu >= n)
								continue;
							cnt[static_cast<size_t>(hu)]--;
							cnt[static_cast<size_t>(big)]++;
							height[static_cast<size_t>(u)] = big;
							dead[static_cast<size_t>(u)] = 1;
						}
					}
				}
			}
		}

		std::vector<char> side(static_cast<size_t>(n), 0);
		for (int v = 0; v < n; v++) {
			if (dead[static_cast<size_t>(v)])
				side[static_cast<size_t>(v)] = 1;
		}
		std::vector<int> stack;
		side[static_cast<size_t>(s)] = 1;
		stack.push_back(s);
		while (!stack.empty()) {
			const int v = stack.back();
			stack.pop_back();
			for (const Edge &e : g[static_cast<size_t>(v)]) {
				if (e.cap > eps && !side[static_cast<size_t>(e.to)]) {
					side[static_cast<size_t>(e.to)] = 1;
					stack.push_back(e.to);
				}
			}
		}
		return side;
	}
};

} // namespace

MaskMagicPick mask_magic_pick(const uint8_t *bgra, int stride, int width, int height, const std::vector<MaskPoint> &loop)
{
	constexpr int k_min = 16;
	constexpr int k_components = 5;
	constexpr int k_iterations = 5;
	constexpr double k_gamma = 50.0;
	constexpr double k_inf = 1e6;
	constexpr double k_cost_cap = 80.0;
	constexpr int k_work_cap = 65536;

	MaskMagicPick pick;
	if (!bgra || width < 3 || height < 3 || width > 16384 || height > 16384 || stride < width * 4 || loop.size() < 3)
		return pick;
	const int n = width * height;
	if (n <= 0)
		return pick;
	pick.width = width;
	pick.height = height;

	std::vector<uint8_t> inside(static_cast<size_t>(n), 0);
	mask_fill_polygon(inside, width, height, loop);
	int inside_count = 0;
	for (int i = 0; i < n; i++) {
		if (inside[static_cast<size_t>(i)])
			inside_count++;
	}
	if (inside_count < k_min) {
		pick.status = MaskMagicPickStatus::TooSmall;
		return pick;
	}

	/* Window: loop bbox expanded by ~15% (min 8px), as in GrabCut practice. */
	const MaskLoopBounds bounds = mask_loop_bounds(loop, width, height, 0);
	if (bounds.empty)
		return pick;
	const int margin = std::max(8, static_cast<int>(std::ceil(0.15 * std::max(bounds.width, bounds.height))));
	const int wx0 = std::max(0, bounds.x - margin);
	const int wy0 = std::max(0, bounds.y - margin);
	const int wx1 = std::min(width - 1, bounds.x + bounds.width - 1 + margin);
	const int wy1 = std::min(height - 1, bounds.y + bounds.height - 1 + margin);
	const int win_w = wx1 - wx0 + 1;
	const int win_h = wy1 - wy0 + 1;
	if (win_w < 3 || win_h < 3)
		return pick;

	int step = 1;
	while (((win_w + step - 1) / step) * ((win_h + step - 1) / step) > k_work_cap)
		step++;
	const int ww = (win_w + step - 1) / step;
	const int wh = (win_h + step - 1) / step;
	const int wn = ww * wh;

	/* Working colours (R,G,B in 0..255) sampled at each cell centre. */
	std::vector<std::array<float, 3>> col(static_cast<size_t>(wn));
	for (int y = 0; y < wh; y++) {
		const int sy = wy0 + std::min(y * step + step / 2, win_h - 1);
		const uint8_t *row = bgra + static_cast<size_t>(sy) * stride;
		for (int x = 0; x < ww; x++) {
			const int sx = wx0 + std::min(x * step + step / 2, win_w - 1);
			col[static_cast<size_t>(y) * ww + x] = {static_cast<float>(row[sx * 4 + 2]),
								static_cast<float>(row[sx * 4 + 1]),
								static_cast<float>(row[sx * 4 + 0])};
		}
	}

	/* Working inside-loop mask: the loop scaled into window coordinates. */
	std::vector<uint8_t> w_in(static_cast<size_t>(wn), 0);
	{
		std::vector<MaskPoint> scaled;
		scaled.reserve(loop.size());
		for (const MaskPoint &pt : loop)
			scaled.push_back({(pt.x - static_cast<float>(wx0)) / static_cast<float>(step),
					  (pt.y - static_cast<float>(wy0)) / static_cast<float>(step)});
		std::vector<uint8_t> filled(static_cast<size_t>(wn), 0);
		mask_fill_polygon(filled, ww, wh, scaled);
		for (int i = 0; i < wn; i++)
			w_in[static_cast<size_t>(i)] = filled[static_cast<size_t>(i)] ? 1 : 0;
	}
	int w_in_count = 0;
	for (int i = 0; i < wn; i++)
		w_in_count += w_in[static_cast<size_t>(i)];
	if (w_in_count < 1)
		return pick;
	if (w_in_count == wn) {
		/* The loop covers the whole window: no background seed at all. */
		pick.status = MaskMagicPickStatus::TooBig;
		return pick;
	}

	/* Contrast in the window: a flat frame has nothing to cut along. */
	double pair_sum = 0;
	int pair_cnt = 0;
	for (int y = 0; y < wh; y++) {
		for (int x = 0; x < ww; x++) {
			const int i = y * ww + x;
			if (x + 1 < ww) {
				double d = 0;
				for (int c = 0; c < 3; c++) {
					const double diff = col[static_cast<size_t>(i)][c] - col[static_cast<size_t>(i + 1)][c];
					d += diff * diff;
				}
				pair_sum += d;
				pair_cnt++;
			}
			if (y + 1 < wh) {
				double d = 0;
				for (int c = 0; c < 3; c++) {
					const double diff = col[static_cast<size_t>(i)][c] - col[static_cast<size_t>(i + ww)][c];
					d += diff * diff;
				}
				pair_sum += d;
				pair_cnt++;
			}
		}
	}
	const double mean_pair = pair_cnt ? pair_sum / pair_cnt : 0;
	if (mean_pair < 1.0) {
		pick.status = MaskMagicPickStatus::NoEdge;
		return pick;
	}
	const double beta = 1.0 / (2.0 * mean_pair);

	/* Neighbour smoothness weights (right and down), constant across cuts. */
	std::vector<double> smooth_r(static_cast<size_t>(wn), 0), smooth_d(static_cast<size_t>(wn), 0);
	for (int y = 0; y < wh; y++) {
		for (int x = 0; x < ww; x++) {
			const int i = y * ww + x;
			if (x + 1 < ww) {
				double d = 0;
				for (int c = 0; c < 3; c++) {
					const double diff = col[static_cast<size_t>(i)][c] - col[static_cast<size_t>(i + 1)][c];
					d += diff * diff;
				}
				smooth_r[static_cast<size_t>(i)] = k_gamma * std::exp(-beta * d);
			}
			if (y + 1 < wh) {
				double d = 0;
				for (int c = 0; c < 3; c++) {
					const double diff = col[static_cast<size_t>(i)][c] - col[static_cast<size_t>(i + ww)][c];
					d += diff * diff;
				}
				smooth_d[static_cast<size_t>(i)] = k_gamma * std::exp(-beta * d);
			}
		}
	}

	/* Trimap: outside the loop is hard background, inside is probable
	 * foreground. As a seed, inside pixels within 2 working pixels of the
	 * loop boundary start as probable background (distance by BFS from
	 * outside): a loose loop's rim is usually background, and seeding the
	 * background model with it stops the foreground model from claiming
	 * the background inside the loop. The cut can flip the rim back when
	 * the element really does fill the loop. */
	std::vector<int> dist(static_cast<size_t>(wn), -1);
	{
		std::vector<int> q;
		for (int i = 0; i < wn; i++) {
			if (w_in[static_cast<size_t>(i)])
				continue;
			dist[static_cast<size_t>(i)] = 0;
			q.push_back(i);
		}
		for (size_t qi = 0; qi < q.size(); qi++) {
			const int i = q[qi];
			const int x = i % ww;
			const int y = i / ww;
			const int nb[4] = {x > 0 ? i - 1 : -1, x + 1 < ww ? i + 1 : -1, y > 0 ? i - ww : -1,
					   y + 1 < wh ? i + ww : -1};
			for (int nbi : nb) {
				if (nbi >= 0 && dist[static_cast<size_t>(nbi)] < 0) {
					dist[static_cast<size_t>(nbi)] = dist[static_cast<size_t>(i)] + 1;
					q.push_back(nbi);
				}
			}
		}
	}
	bool any_deep = false;
	for (int i = 0; i < wn; i++) {
		if (w_in[static_cast<size_t>(i)] && dist[static_cast<size_t>(i)] > 2)
			any_deep = true;
	}
	std::vector<uint8_t> label(static_cast<size_t>(wn));
	for (int i = 0; i < wn; i++)
		label[static_cast<size_t>(i)] =
			(w_in[static_cast<size_t>(i)] && (!any_deep || dist[static_cast<size_t>(i)] > 2)) ? 1 : 0;

	GcGmm fg_model, bg_model;
	const int source = wn;
	const int sink = wn + 1;
	for (int it = 0; it < k_iterations; it++) {
		std::vector<std::array<float, 3>> fg_samples, bg_samples;
		for (int i = 0; i < wn; i++) {
			if (label[static_cast<size_t>(i)])
				fg_samples.push_back(col[static_cast<size_t>(i)]);
			else
				bg_samples.push_back(col[static_cast<size_t>(i)]);
		}
		fg_model = gc_fit_gmm(fg_samples, k_components);
		bg_model = gc_fit_gmm(bg_samples, k_components);
		if (fg_model.empty() || bg_model.empty()) {
			pick.status = MaskMagicPickStatus::NoEdge;
			return pick;
		}

		GcGraph graph(wn + 2);
		for (int y = 0; y < wh; y++) {
			for (int x = 0; x < ww; x++) {
				const int i = y * ww + x;
				double d_fg, d_bg;
				if (!w_in[static_cast<size_t>(i)]) {
					d_fg = k_inf;
					d_bg = 0;
					graph.keep[static_cast<size_t>(i)] = 1;
				} else {
					const float *z = col[static_cast<size_t>(i)].data();
					const double raw_fg = fg_model.neglog(z);
					const double raw_bg = bg_model.neglog(z);
					const double lo = std::min(raw_fg, raw_bg);
					d_fg = std::min(raw_fg - lo, k_cost_cap);
					d_bg = std::min(raw_bg - lo, k_cost_cap);
				}
				/* Source side = foreground: cutting source->i costs
				 * the background cost, cutting i->sink costs the
				 * foreground cost. */
				if (d_bg > 0)
					graph.add_directed(source, i, d_bg);
				if (d_fg > 0)
					graph.add_directed(i, sink, d_fg);
				if (x + 1 < ww && smooth_r[static_cast<size_t>(i)] > 1e-9)
					graph.add_cut_edge(i, i + 1, smooth_r[static_cast<size_t>(i)]);
				if (y + 1 < wh && smooth_d[static_cast<size_t>(i)] > 1e-9)
					graph.add_cut_edge(i, i + ww, smooth_d[static_cast<size_t>(i)]);
			}
		}
		const std::vector<char> side = graph.mincut_source_side(source, sink);
		int changed = 0;
		for (int i = 0; i < wn; i++) {
			const uint8_t nl = (!w_in[static_cast<size_t>(i)] || side[static_cast<size_t>(i)]) ? 1 : 0;
			/* Hard background stays background whatever the cut did. */
			const uint8_t fixed = w_in[static_cast<size_t>(i)] ? nl : 0;
			if (fixed != label[static_cast<size_t>(i)])
				changed++;
			label[static_cast<size_t>(i)] = fixed;
		}
		if (changed * 500 < wn)
			break;
	}

	/* Fill holes enclosed inside the loop: flood in from the window border
	 * through background; inside-loop pixels it cannot reach are holes. */
	{
		std::vector<uint8_t> reach(static_cast<size_t>(wn), 0);
		std::vector<int> flood;
		auto seed = [&](int i) {
			if (!label[static_cast<size_t>(i)] && !reach[static_cast<size_t>(i)]) {
				reach[static_cast<size_t>(i)] = 1;
				flood.push_back(i);
			}
		};
		for (int x = 0; x < ww; x++) {
			seed(x);
			seed((wh - 1) * ww + x);
		}
		for (int y = 0; y < wh; y++) {
			seed(y * ww);
			seed(y * ww + ww - 1);
		}
		for (size_t qi = 0; qi < flood.size(); qi++) {
			const int i = flood[qi];
			const int x = i % ww;
			const int y = i / ww;
			if (x > 0)
				seed(i - 1);
			if (x + 1 < ww)
				seed(i + 1);
			if (y > 0)
				seed(i - ww);
			if (y + 1 < wh)
				seed(i + ww);
		}
		for (int i = 0; i < wn; i++) {
			if (w_in[static_cast<size_t>(i)] && !reach[static_cast<size_t>(i)])
				label[static_cast<size_t>(i)] = 1;
		}
	}

	/* Map back to full resolution (nearest), then re-decide a 1-cell band
	 * around the boundary with the final colour models when downsampled. */
	std::vector<uint8_t> solid(static_cast<size_t>(n), 0);
	for (int y = 0; y < win_h; y++) {
		const int wy = std::min(y / step, wh - 1);
		for (int x = 0; x < win_w; x++) {
			const int wx = std::min(x / step, ww - 1);
			if (label[static_cast<size_t>(wy) * ww + wx])
				solid[static_cast<size_t>(wy0 + y) * width + (wx0 + x)] = 1;
		}
	}
	if (step > 1 && !fg_model.empty() && !bg_model.empty()) {
		const std::vector<uint8_t> up = solid;
		for (int y = 0; y < win_h; y++) {
			for (int x = 0; x < win_w; x++) {
				const int fi = (wy0 + y) * width + (wx0 + x);
				if (!inside[static_cast<size_t>(fi)])
					continue;
				bool boundary = false;
				if (x > 0 && up[static_cast<size_t>(fi - 1)] != up[static_cast<size_t>(fi)])
					boundary = true;
				if (x + 1 < win_w && up[static_cast<size_t>(fi + 1)] != up[static_cast<size_t>(fi)])
					boundary = true;
				if (y > 0 && up[static_cast<size_t>(fi - width)] != up[static_cast<size_t>(fi)])
					boundary = true;
				if (y + 1 < win_h && up[static_cast<size_t>(fi + width)] != up[static_cast<size_t>(fi)])
					boundary = true;
				if (!boundary)
					continue;
				const uint8_t *px = bgra + static_cast<size_t>(wy0 + y) * stride + static_cast<size_t>(wx0 + x) * 4;
				const float z[3] = {static_cast<float>(px[2]), static_cast<float>(px[1]),
						    static_cast<float>(px[0])};
				solid[static_cast<size_t>(fi)] = fg_model.neglog(z) <= bg_model.neglog(z) ? 1 : 0;
			}
		}
	}

	int mask_pixels = 0;
	for (int i = 0; i < n; i++) {
		if (solid[static_cast<size_t>(i)] && inside[static_cast<size_t>(i)])
			mask_pixels++;
	}
	if (mask_pixels == 0) {
		pick.status = MaskMagicPickStatus::NoEdge;
		return pick;
	}
	if (mask_pixels < k_min || static_cast<int64_t>(mask_pixels) * 1000 < static_cast<int64_t>(inside_count) * 4) {
		pick.status = MaskMagicPickStatus::TooSmall;
		return pick;
	}
	if (static_cast<int64_t>(mask_pixels) * 100 > static_cast<int64_t>(inside_count) * 97) {
		pick.status = MaskMagicPickStatus::TooBig;
		return pick;
	}
	pick.gray.assign(static_cast<size_t>(n), 0);
	for (int i = 0; i < n; i++) {
		if (solid[static_cast<size_t>(i)] && inside[static_cast<size_t>(i)])
			pick.gray[static_cast<size_t>(i)] = 255;
	}
	pick.status = MaskMagicPickStatus::Ok;
	return pick;
}

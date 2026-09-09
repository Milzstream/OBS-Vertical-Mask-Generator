#include "mask-process.hpp"
#include "update-parse.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_fails = 0;

#define CHECK(cond)                                                                                                    \
	do {                                                                                                           \
		if (!(cond)) {                                                                                         \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                    \
			g_fails++;                                                                                     \
		}                                                                                                      \
	} while (0)

static int count_opaque(const std::vector<uint8_t> &g)
{
	int n = 0;
	for (uint8_t p : g) {
		if (p >= 128)
			n++;
	}
	return n;
}

static std::vector<uint8_t> filled_square(int w, int h, int x0, int y0, int x1, int y1)
{
	std::vector<uint8_t> g(static_cast<size_t>(w) * h, 0);
	for (int y = y0; y <= y1; y++)
		for (int x = x0; x <= x1; x++)
			g[static_cast<size_t>(y) * w + x] = 255;
	return g;
}

static std::vector<uint8_t> hollow_rect(int w, int h, int x0, int y0, int x1, int y1, uint8_t v = 40)
{
	std::vector<uint8_t> g(static_cast<size_t>(w) * h, 0);
	for (int y = y0; y <= y1; y++) {
		g[static_cast<size_t>(y) * w + x0] = v;
		g[static_cast<size_t>(y) * w + x1] = v;
	}
	for (int x = x0; x <= x1; x++) {
		g[static_cast<size_t>(y0) * w + x] = v;
		g[static_cast<size_t>(y1) * w + x] = v;
	}
	return g;
}

int main()
{
	/* Expand grows the opaque region. */
	{
		auto g = filled_square(32, 32, 10, 10, 21, 21);
		const int before = count_opaque(g);
		mask_expand(g, 32, 32, 2);
		CHECK(count_opaque(g) > before);
	}

	/* Negative expand shrinks. */
	{
		auto g = filled_square(32, 32, 8, 8, 23, 23);
		const int before = count_opaque(g);
		mask_expand(g, 32, 32, -2);
		CHECK(count_opaque(g) < before);
		CHECK(count_opaque(g) > 0);
	}

	/* Expand 0 is a no-op. */
	{
		auto g = filled_square(16, 16, 4, 4, 11, 11);
		auto copy = g;
		mask_expand(g, 16, 16, 0);
		CHECK(g == copy);
	}

	/* Feather produces in-between values on the edge. */
	{
		auto g = filled_square(32, 32, 8, 8, 23, 23);
		mask_feather(g, 32, 32, 4);
		bool mid = false;
		bool core = false;
		bool outside = false;
		for (uint8_t p : g) {
			if (p > 0 && p < 255)
				mid = true;
			if (p == 255)
				core = true;
			if (p == 0)
				outside = true;
		}
		CHECK(mid);
		CHECK(core);
		CHECK(outside);
	}

	/* Binarize splits at the threshold. */
	{
		std::vector<uint8_t> g{0, 127, 128, 255};
		mask_binarize(g, 128);
		CHECK(g[0] == 0);
		CHECK(g[1] == 0);
		CHECK(g[2] == 255);
		CHECK(g[3] == 255);
	}

	/* Flood-fill a hollow ring from the center, absorbing the outline. */
	{
		auto g = hollow_rect(24, 24, 4, 4, 19, 19);
		CHECK(mask_flood_fill(g, 24, 24, 12, 12, true));
		CHECK(g[static_cast<size_t>(12) * 24 + 12] == 255);
		CHECK(g[static_cast<size_t>(12) * 24 + 4] == 255);
		CHECK(g[static_cast<size_t>(4) * 24 + 4] == 255);
		CHECK(g[static_cast<size_t>(4) * 24 + 12] == 255);
		CHECK(g[static_cast<size_t>(19) * 24 + 19] == 255);
		CHECK(g[static_cast<size_t>(0) * 24 + 0] == 0);
		CHECK(g[static_cast<size_t>(12) * 24 + 3] == 0);
	}

	/* Clicking a wall does not fill. */
	{
		auto g = filled_square(16, 16, 2, 2, 13, 13);
		auto copy = g;
		CHECK(!mask_flood_fill(g, 16, 16, 2, 2, true));
		CHECK(g == copy);
	}

	/* Out of bounds seed fails. */
	{
		auto g = filled_square(8, 8, 1, 1, 6, 6);
		CHECK(!mask_flood_fill(g, 8, 8, -1, 0, true));
		CHECK(!mask_flood_fill(g, 8, 8, 0, 8, true));
		CHECK(!mask_flood_erase(g, 8, 8, -1, 0));
		CHECK(!mask_flood_erase(g, 8, 8, 8, 0));
	}

	/* Too-small buffer is rejected. */
	{
		std::vector<uint8_t> g(4, 0);
		CHECK(!mask_flood_fill(g, 8, 8, 1, 1, true));
		CHECK(!mask_flood_erase(g, 8, 8, 1, 1));
		CHECK(!mask_flood_fill(g, 0, 8, 0, 0, true));
	}

	/* Without absorb, a dim outline stays dim. */
	{
		auto g = hollow_rect(20, 20, 3, 3, 16, 16);
		CHECK(mask_flood_fill(g, 20, 20, 10, 10, false));
		CHECK(g[static_cast<size_t>(10) * 20 + 10] == 255);
		CHECK(g[static_cast<size_t>(3) * 20 + 3] == 40);
		CHECK(g[static_cast<size_t>(3) * 20 + 10] == 40);
		CHECK(g[static_cast<size_t>(0) * 20 + 0] == 0);
	}

	/* A solid barrier stops the fill from leaking. */
	{
		std::vector<uint8_t> g(static_cast<size_t>(16) * 16, 0);
		for (int y = 0; y < 16; y++)
			g[static_cast<size_t>(y) * 16 + 8] = 255;
		CHECK(mask_flood_fill(g, 16, 16, 2, 8, true));
		CHECK(g[static_cast<size_t>(8) * 16 + 2] == 255);
		CHECK(g[static_cast<size_t>(8) * 16 + 12] == 0);
	}

	/* Empty canvas fills everything. */
	{
		std::vector<uint8_t> g(static_cast<size_t>(8) * 8, 0);
		CHECK(mask_flood_fill(g, 8, 8, 0, 0, true));
		CHECK(count_opaque(g) == 64);
	}

	/* A 1px hole in the outline does not leak; absorb seals it. */
	{
		auto g = hollow_rect(20, 20, 3, 3, 16, 16);
		g[static_cast<size_t>(3) * 20 + 10] = 0;
		CHECK(mask_flood_fill(g, 20, 20, 10, 10, true));
		CHECK(g[static_cast<size_t>(10) * 20 + 10] == 255);
		CHECK(g[static_cast<size_t>(3) * 20 + 10] == 255);
		CHECK(g[static_cast<size_t>(0) * 20 + 10] == 0);
		CHECK(g[static_cast<size_t>(2) * 20 + 10] == 0);
	}

	/* Without absorb, the 1px hole stays empty and still does not leak. */
	{
		auto g = hollow_rect(20, 20, 3, 3, 16, 16);
		g[static_cast<size_t>(3) * 20 + 10] = 0;
		CHECK(mask_flood_fill(g, 20, 20, 10, 10, false));
		CHECK(g[static_cast<size_t>(10) * 20 + 10] == 255);
		CHECK(g[static_cast<size_t>(3) * 20 + 10] == 0);
		CHECK(g[static_cast<size_t>(2) * 20 + 10] == 0);
	}

	/* A 2px opening is a real gap: fill leaks out. */
	{
		auto g = hollow_rect(20, 20, 3, 3, 16, 16);
		g[static_cast<size_t>(3) * 20 + 10] = 0;
		g[static_cast<size_t>(3) * 20 + 11] = 0;
		CHECK(mask_flood_fill(g, 20, 20, 10, 10, true));
		CHECK(g[static_cast<size_t>(0) * 20 + 0] == 255);
	}

	/* Erase clears a painted blob and stops at empty. */
	{
		auto g = filled_square(16, 16, 4, 4, 11, 11);
		CHECK(mask_flood_erase(g, 16, 16, 6, 6));
		CHECK(count_opaque(g) == 0);
	}

	/* Erase on empty pixels is a no-op. */
	{
		auto g = filled_square(12, 12, 3, 3, 8, 8);
		auto copy = g;
		CHECK(!mask_flood_erase(g, 12, 12, 0, 0));
		CHECK(g == copy);
	}

	/* Erase crosses a 1px gap between two painted blobs. */
	{
		auto g = filled_square(16, 16, 2, 2, 6, 6);
		for (int y = 2; y <= 6; y++)
			for (int x = 8; x <= 12; x++)
				g[static_cast<size_t>(y) * 16 + x] = 255;
		CHECK(mask_flood_erase(g, 16, 16, 4, 4));
		CHECK(g[static_cast<size_t>(4) * 16 + 4] == 0);
		CHECK(g[static_cast<size_t>(4) * 16 + 10] == 0);
	}

	/* Erase does not jump a 2px gap. */
	{
		auto g = filled_square(16, 16, 2, 2, 6, 6);
		for (int y = 2; y <= 6; y++)
			for (int x = 9; x <= 13; x++)
				g[static_cast<size_t>(y) * 16 + x] = 255;
		CHECK(mask_flood_erase(g, 16, 16, 4, 4));
		CHECK(g[static_cast<size_t>(4) * 16 + 4] == 0);
		CHECK(g[static_cast<size_t>(4) * 16 + 11] == 255);
	}

	/* Expand of empty stays empty; expand 0 is already covered. */
	{
		std::vector<uint8_t> g(static_cast<size_t>(8) * 8, 0);
		mask_expand(g, 8, 8, 2);
		CHECK(count_opaque(g) == 0);
	}

	/* Feather 0 is a no-op. */
	{
		auto g = filled_square(16, 16, 4, 4, 11, 11);
		auto copy = g;
		mask_feather(g, 16, 16, 0);
		CHECK(g == copy);
	}

	{
		MaskVer a{};
		MaskVer b{};
		CHECK(mask_parse_ver("0.1.0", a));
		CHECK(mask_parse_ver("v0.1.1", b));
		CHECK(a.maj == 0 && a.min == 1 && a.pat == 0);
		CHECK(b.maj == 0 && b.min == 1 && b.pat == 1);
		CHECK(mask_cmp_ver(b, a) > 0);
		CHECK(mask_cmp_ver(a, a) == 0);
		CHECK(mask_cmp_ver(a, b) < 0);
		CHECK(mask_parse_ver("1.2", a));
		CHECK(a.maj == 1 && a.min == 2 && a.pat == 0);
		CHECK(!mask_parse_ver("", a));
		CHECK(!mask_parse_ver(nullptr, a));
	}

	{
		const std::string json =
			"{\n  \"tag_name\": \"0.1.1\",\n  \"prerelease\": false,\n"
			"  \"html_url\": \"https://github.com/example/repo/releases/tag/0.1.1\",\n"
			"  \"assets\": [\n"
			"    {\"browser_download_url\": "
			"\"https://github.com/example/repo/releases/download/0.1.1/"
			"plugin-0.1.1-source.zip\"},\n"
			"    {\"browser_download_url\": "
			"\"https://github.com/example/repo/releases/download/0.1.1/"
			"plugin-0.1.1-windows-x64.zip\"},\n"
			"    {\"browser_download_url\": "
			"\"https://github.com/example/repo/releases/download/0.1.1/"
			"plugin-0.1.1-windows-x64.exe\"}\n"
			"  ]\n}";
		std::string tag;
		std::string page;
		std::string asset;
		bool pre = true;
		CHECK(mask_json_string_field(json, "tag_name", tag) && tag == "0.1.1");
		CHECK(mask_json_string_field(json, "html_url", page) && page.find("0.1.1") != std::string::npos);
		CHECK(mask_json_bool_field(json, "prerelease", pre) && !pre);
		mask_find_windows_asset(json, asset);
		CHECK(asset.size() >= 4 && asset.compare(asset.size() - 4, 4, ".exe") == 0);
		CHECK(asset.find("windows") != std::string::npos);
	}

	/* Crop insets come from the opaque bbox plus pad. */
	{
		auto g = filled_square(32, 32, 8, 8, 15, 15);
		const MaskCrop c = mask_crop_from_opaque(g, 32, 32, 20, 2);
		CHECK(!c.empty);
		CHECK(c.min_x == 6 && c.min_y == 6);
		CHECK(c.max_x == 17 && c.max_y == 17);
		CHECK(c.left == 6 && c.top == 6);
		CHECK(c.right == 14 && c.bottom == 14);
		std::vector<uint8_t> empty(static_cast<size_t>(8) * 8, 0);
		CHECK(mask_crop_from_opaque(empty, 8, 8, 20, 32).empty);
	}

	/* Snap a painted ring onto a high-contrast box, not an inner icon. */
	{
		const int w = 64, h = 64;
		std::vector<uint8_t> lum(static_cast<size_t>(w) * h, 0);
		for (int y = 12; y <= 51; y++) {
			for (int x = 12; x <= 51; x++) {
				const bool frame = x <= 14 || x >= 49 || y <= 14 || y >= 49;
				if (frame)
					lum[static_cast<size_t>(y) * w + x] = 255;
			}
		}
		for (int y = 30; y <= 33; y++)
			for (int x = 30; x <= 33; x++)
				lum[static_cast<size_t>(y) * w + x] = 255;

		std::vector<uint8_t> user(static_cast<size_t>(w) * h, 0);
		for (int y = 10; y <= 53; y++) {
			for (int x = 10; x <= 53; x++) {
				const bool ring = x <= 16 || x >= 47 || y <= 16 || y >= 47;
				if (ring)
					user[static_cast<size_t>(y) * w + x] = 255;
			}
		}

		std::vector<uint8_t> out;
		CHECK(mask_snap_edges(user, lum, w, h, 8, out));
		CHECK(static_cast<int>(out.size()) == w * h);

		int minx = w, miny = h, maxx = -1, maxy = -1;
		int opaque = 0;
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				if (out[static_cast<size_t>(y) * w + x] < 128)
					continue;
				opaque++;
				minx = std::min(minx, x);
				miny = std::min(miny, y);
				maxx = std::max(maxx, x);
				maxy = std::max(maxy, y);
			}
		}
		CHECK(opaque > 200);
		CHECK(minx <= 16 && miny <= 16);
		CHECK(maxx >= 47 && maxy >= 47);
		CHECK(!(minx >= 28 && maxx <= 36 && miny >= 28 && maxy <= 36));
		CHECK(out[static_cast<size_t>(2) * w + 2] < 128);
	}

	/* Magic loop around a filled rectangle hugs the outer edge, not an inner hole. */
	{
		const int w = 64, h = 64;
		std::vector<uint8_t> lum(static_cast<size_t>(w) * h, 0);
		for (int y = 16; y <= 47; y++)
			for (int x = 16; x <= 47; x++)
				lum[static_cast<size_t>(y) * w + x] = 255;
		for (int y = 26; y <= 37; y++)
			for (int x = 26; x <= 37; x++)
				lum[static_cast<size_t>(y) * w + x] = 0;

		std::vector<MaskPoint> loop = {{8.f, 8.f}, {55.f, 8.f}, {55.f, 55.f}, {8.f, 55.f},
					       {8.f, 8.f}};
		/* densify a bit so shrinkwrap has enough samples */
		std::vector<MaskPoint> dense;
		for (size_t i = 1; i < loop.size(); i++) {
			for (int k = 0; k < 16; k++) {
				const float t = static_cast<float>(k) / 16.f;
				dense.push_back({loop[i - 1].x + t * (loop[i].x - loop[i - 1].x),
						 loop[i - 1].y + t * (loop[i].y - loop[i - 1].y)});
			}
		}
		std::vector<MaskPoint> snapped;
		CHECK(mask_magic_shrinkwrap(dense, lum, w, h, 18, snapped));
		CHECK(snapped.size() >= 8);

		int near_outer = 0;
		int in_hole = 0;
		for (const MaskPoint &pt : snapped) {
			const int x = static_cast<int>(std::lround(pt.x));
			const int y = static_cast<int>(std::lround(pt.y));
			if (x >= 26 && x <= 37 && y >= 26 && y <= 37)
				in_hole++;
			const int d_outer = std::min(std::min(std::abs(x - 16), std::abs(x - 47)),
						     std::min(std::abs(y - 16), std::abs(y - 47)));
			if (d_outer <= 4)
				near_outer++;
		}
		CHECK(in_hole == 0);
		CHECK(near_outer * 2 > static_cast<int>(snapped.size()));
	}

	CHECK(mask_presence_threshold(0) == 0.0f);
	CHECK(mask_presence_threshold(50) == 0.5f);
	CHECK(mask_presence_threshold(80) == 0.8f);
	CHECK(mask_presence_threshold(100) == 1.0f);
	CHECK(mask_presence_threshold(50) > mask_presence_threshold(0));
	CHECK(mask_presence_threshold(50) < mask_presence_threshold(100));

	/* Presence: painted outline vs live edges. Interior color/art can change. */
	{
		const int w = 48, h = 48;
		std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
		for (int y = 8; y <= 39; y++)
			for (int x = 8; x <= 39; x++)
				mask[static_cast<size_t>(y) * w + x] = 255;

		auto panel = [&](uint8_t border, uint8_t fill, int x0, int y0, int x1, int y1) {
			std::vector<uint8_t> g(static_cast<size_t>(w) * h, 40);
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) {
					const bool rim = x <= x0 + 2 || x >= x1 - 2 || y <= y0 + 2 || y >= y1 - 2;
					g[static_cast<size_t>(y) * w + x] = rim ? border : fill;
				}
			}
			return g;
		};

		std::vector<uint8_t> gold = panel(220, 90, 8, 8, 39, 39);
		float score = 0;
		CHECK(mask_presence_outline_score(mask, gold, nullptr, w, h, &score));
		CHECK(score > mask_presence_threshold(50));

		/* Legendary recolor + different gun art. */
		std::vector<uint8_t> red = panel(40, 200, 8, 8, 39, 39);
		for (int y = 16; y <= 28; y++)
			for (int x = 14; x <= 34; x++)
				red[static_cast<size_t>(y) * w + x] = 240;
		CHECK(mask_presence_outline_score(mask, red, nullptr, w, h, &score));
		CHECK(score > mask_presence_threshold(50));

		/* Empty slot: same hex, smaller and fainter. */
		std::vector<uint8_t> empty = panel(160, 50, 12, 12, 35, 35);
		CHECK(mask_presence_outline_score(mask, empty, nullptr, w, h, &score));
		CHECK(score > mask_presence_threshold(50));

		/* HUD gone: no hex, just a smooth world (sky / wall). */
		std::vector<uint8_t> gone(static_cast<size_t>(w) * h);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				gone[static_cast<size_t>(y) * w + x] =
					static_cast<uint8_t>(60 + (x + y) * 80 / (w + h));
		CHECK(mask_presence_outline_score(mask, gone, nullptr, w, h, &score));
		CHECK(score < mask_presence_threshold(50));

		/* World moving through the slot: edges do not stay put. */
		std::vector<uint8_t> world0(static_cast<size_t>(w) * h);
		std::vector<uint8_t> world1(static_cast<size_t>(w) * h);
		for (int i = 0; i < w * h; i++) {
			world0[static_cast<size_t>(i)] = static_cast<uint8_t>((i * 13) & 255);
			world1[static_cast<size_t>(i)] = static_cast<uint8_t>(((i + 17) * 29) & 255);
		}
		CHECK(mask_presence_outline_score(mask, world1, &world0, w, h, &score));
		CHECK(score < mask_presence_threshold(50));
	}

	if (g_fails) {
		std::printf("%d check(s) failed\n", g_fails);
		return 1;
	}
	std::printf("mask-process-tests: all checks passed\n");
	return 0;
}

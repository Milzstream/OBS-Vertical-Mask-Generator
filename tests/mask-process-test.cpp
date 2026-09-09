#include "mask-process.hpp"

#include <cstdio>
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

	CHECK(mask_is_roundish(100, 100, 7800));
	CHECK(!mask_is_roundish(200, 40, 7000));
	CHECK(!mask_is_roundish(0, 10, 1));
	CHECK(mask_circle_radius_from_bounds(100, 100) == 50);
	CHECK(mask_circle_radius_from_bounds(80, 100) == 40);

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

	if (g_fails) {
		std::printf("%d check(s) failed\n", g_fails);
		return 1;
	}
	std::printf("mask-process-tests: all checks passed\n");
	return 0;
}

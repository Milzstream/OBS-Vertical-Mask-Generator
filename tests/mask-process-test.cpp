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

	if (g_fails) {
		std::printf("%d check(s) failed\n", g_fails);
		return 1;
	}
	std::printf("mask-process-tests: all checks passed\n");
	return 0;
}

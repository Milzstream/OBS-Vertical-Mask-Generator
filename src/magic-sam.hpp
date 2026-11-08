#pragma once

#include "mask-process.hpp"

#include <cstdint>
#include <vector>

/* Prompted MobileSAM pick. Magic Select does not call this. Kept until the
 * ONNX stack is removed. Unavailable means the runtime or models are missing.
 * Rejected means the model ran and the pick was not trusted. */

enum class MagicSamStatus { Ok, Unavailable, Rejected, Failed };

struct MagicSamPick {
	MagicSamStatus status = MagicSamStatus::Failed;
	float iou = 0;
	bool used_fallback = false;
	std::vector<uint8_t> gray;
	int width = 0;
	int height = 0;
};

/* bgra is tightly or strided BGRA8 (Qt ARGB32 on little-endian). */
MagicSamPick magic_sam_select(const uint8_t *bgra, int stride, int width, int height,
			      const std::vector<MaskPoint> &loop);

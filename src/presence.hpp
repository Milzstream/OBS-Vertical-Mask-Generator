#pragma once

#include <cstdint>

struct hud_mask;

void hud_mask_presence_start(void);
void hud_mask_presence_stop(void);
void hud_mask_presence_register(hud_mask *ctx);
void hud_mask_presence_unregister(hud_mask *ctx);
void hud_mask_presence_load_ref(hud_mask *ctx);
void hud_mask_presence_clear_ref(hud_mask *ctx);
bool hud_mask_presence_save_ref(hud_mask *ctx, const uint8_t *luma, const uint8_t *mask_gray, int width, int height);
void hud_mask_presence_tick_fade(hud_mask *ctx, float seconds);

#pragma once

#include <cstdint>
#include <string>

struct hud_mask;

void hud_mask_presence_start(void);
void hud_mask_presence_stop(void);
void hud_mask_presence_register(hud_mask *ctx);
void hud_mask_presence_unregister(hud_mask *ctx);
std::string hud_mask_presence_still_path(const std::string &mask_path);
std::string hud_mask_presence_ref_path(const std::string &mask_path);
void hud_mask_presence_load_ref(hud_mask *ctx);
void hud_mask_presence_clear_ref(hud_mask *ctx);
bool hud_mask_presence_save_ref(hud_mask *ctx, const uint8_t *luma, const uint8_t *mask_gray, int width, int height);
bool hud_mask_presence_drop_extra(hud_mask *ctx, int index);
void hud_mask_presence_tick_fade(hud_mask *ctx, float seconds);

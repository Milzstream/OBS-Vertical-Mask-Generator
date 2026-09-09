/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "hud-mask.hpp"
#include "presence.hpp"
#include "update-check.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("HUDMask.Description");
}

bool obs_module_load(void)
{
	hud_mask_load_effects();
	register_hud_mask_source();
	hud_mask_presence_start();
	hud_mask_update_check_start();
	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	hud_mask_update_check_stop();
	hud_mask_presence_stop();
	hud_mask_unload_effects();
	obs_log(LOG_INFO, "unloaded");
}

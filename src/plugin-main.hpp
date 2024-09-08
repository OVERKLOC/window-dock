#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("custom_window_docks", "en-US")

bool obs_module_load(void);
void obs_module_unload(void);
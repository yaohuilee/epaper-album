#pragma once
#include "esp_err.h"

/* Future operations report NOT_SUPPORTED until implemented; no false success. */
esp_err_t epaper_album_next(void);
esp_err_t epaper_display_bmp(const char *sd_path);
esp_err_t epaper_announce_now(void);
esp_err_t epaper_activity_start(void);

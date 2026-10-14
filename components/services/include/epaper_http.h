#pragma once
#include "esp_err.h"
#include <stdbool.h>
esp_err_t epaper_http_start(void);
void epaper_http_stop(void);
bool epaper_http_running(void);
/* Called from app_main; handlers serve a protected immutable JSON snapshot. */
void epaper_http_update(bool usb_host);
/* Run accepted commands on app_main, serialized with display and SD access. */
bool epaper_http_process(void);
/* Shared with the main task: excludes filesystem transfers and display/config access. */
bool epaper_http_sd_try_lock(void);
void epaper_http_sd_unlock(void);

unsigned epaper_http_next_revision(void);

#pragma once
#include "esp_err.h"
#include "cJSON.h"

#define EPAPER_SD_MOUNT "/sdcard"
esp_err_t epaper_storage_mount(void);
esp_err_t epaper_storage_unmount(void);
/* Envelope validation only. Caller owns *out and must cJSON_Delete it. */
esp_err_t epaper_config_read(cJSON **out);

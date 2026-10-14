#pragma once
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>
#include <stdint.h>
esp_err_t epaper_announcement_start(void);
bool epaper_announcement_busy(void);
bool epaper_announcement_prepare_sleep(void);
bool epaper_announcement_admit(unsigned seconds);
uint64_t epaper_announcement_sleep_us(void);
void epaper_announcement_json(cJSON *root);
esp_err_t epaper_announcement_settings(const char *request, bool periods, char *error, size_t size);
esp_err_t epaper_announcement_test(const char *request, char *error, size_t size);

#pragma once
#include "esp_err.h"
#include "epaper_core.h"

typedef struct { epaper_calendar_t calendar; bool stopped; bool valid; } epaper_rtc_t;
typedef struct { float temperature_c; float humidity_percent; } epaper_environment_t;
esp_err_t epaper_rtc_read(epaper_rtc_t *out);
esp_err_t epaper_rtc_write(const epaper_calendar_t *utc);
esp_err_t epaper_environment_read(epaper_environment_t *out);
esp_err_t epaper_environment_initialize(void);
esp_err_t epaper_power_read(epaper_power_t *out);

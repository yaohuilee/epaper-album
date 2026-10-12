#pragma once
#include "epaper_album.h"
#include "esp_err.h"
esp_err_t epaper_wifi_provision(void);
void epaper_time_start(const epaper_time_config_t *config,bool deep_wake);
bool epaper_time_due(void);
uint64_t epaper_time_sleep_us(void);
esp_err_t epaper_time_sync(void);
bool epaper_time_rtc_utc(void);
typedef struct {
    bool enabled,attempted;
    esp_err_t result;
    int64_t last_attempt,last_success;
    char last_server[128];
} epaper_time_status_t;
void epaper_time_status(epaper_time_status_t *out);
bool epaper_time_enabled(void);
void epaper_time_reconfigure(const epaper_time_config_t *config);
void epaper_time_update_network(const epaper_time_config_t *config);

void epaper_time_network_hold(bool hold);

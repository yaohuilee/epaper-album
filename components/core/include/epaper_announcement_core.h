#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
cJSON *epaper_announcement_json_parse(const char *text, size_t limit);
#define ANNOUNCEMENT_MAX_PERIODS 8
#define ANNOUNCEMENT_RATE 24000
#define ANNOUNCEMENT_PREPARE_SECONDS 20
bool epaper_announcement_countdown_window(int64_t lead_us);
uint64_t epaper_announcement_wake_delay(int64_t target_epoch,int64_t now_us);
typedef struct
{
    char id[33], name[97];
    bool enabled;
    unsigned start, end;
} epaper_pause_period_t;
typedef struct
{
    bool enabled, hourly, half_hourly, temperature, humidity;
    unsigned volume, temp_decimals, period_count;
    epaper_pause_period_t periods[ANNOUNCEMENT_MAX_PERIODS];
} epaper_announcement_config_t;
bool epaper_announcement_parse(const char *global, epaper_announcement_config_t *out, char *error,
                               size_t size);
bool epaper_announcement_edit(const char *global, const char *request, bool periods, char **output,
                              epaper_announcement_config_t *out, char *error, size_t size);
bool epaper_announcement_paused(const epaper_announcement_config_t *config, int64_t epoch,
                                int *period);
int64_t epaper_announcement_next(const epaper_announcement_config_t *config, int64_t minimum,
                                 int64_t processed);
bool epaper_audio_pack_name(const char *name);
bool epaper_announcement_migrate(const char *global, char **output, char *error, size_t size);

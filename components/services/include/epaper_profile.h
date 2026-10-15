#pragma once
#include "cJSON.h"
#include "esp_err.h"
#include <stdbool.h>
typedef enum {PROFILE_SYNTHESIS, PROFILE_PLAYBACK, PROFILE_PHOTO, PROFILE_TRANSFER,
              PROFILE_NTP, PROFILE_FAULT, PROFILE_IMPORT, PROFILE_IDLE, PROFILE_COUNT} epaper_profile_phase_t;
void epaper_profile_start(void);
bool epaper_profile_available(void);
void epaper_profile_phase(epaper_profile_phase_t phase, bool active);
bool epaper_profile_reset(void);
void epaper_profile_json(cJSON *root);

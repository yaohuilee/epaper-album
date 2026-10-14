#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
esp_err_t epaper_audio_output_prepare(void);
esp_err_t epaper_audio_output_play(const int16_t *pcm, size_t samples, unsigned volume,
                                   int64_t start_us);
void epaper_audio_output_close(void);

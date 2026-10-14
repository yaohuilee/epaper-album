#pragma once
#include "epaper_announcement_core.h"
typedef bool (*epaper_speech_synthesizer_t)(const char *name, int16_t **samples, size_t *count,
                                     void *context);
typedef struct
{
    int16_t *pcm;
    size_t samples, capacity;
    size_t last_beep_start, last_beep_end, prefix_start, prefix_end, time_start, time_end,
        environment_start;
    unsigned beep_count;
} epaper_audio_sequence_t;
bool epaper_audio_sequence_build(epaper_audio_sequence_t *sequence, unsigned hour, bool half,
                                 const epaper_announcement_config_t *config, bool environment_valid,
                                 float temperature, float humidity, epaper_speech_synthesizer_t load,
                                 void *context);
void epaper_audio_sequence_free(epaper_audio_sequence_t *sequence);

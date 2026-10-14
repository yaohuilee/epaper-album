#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool epaper_tts_available(void);
const char *epaper_tts_voice_name(void);
bool epaper_tts_synthesize(const char *text, int16_t **pcm, size_t *samples, void *context);
void epaper_tts_close(void);

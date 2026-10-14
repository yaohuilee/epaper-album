#pragma once
#include <stddef.h>
#include <stdint.h>
size_t epaper_pcm_16_to_24(const int16_t *input, size_t count, int16_t *output, size_t capacity);

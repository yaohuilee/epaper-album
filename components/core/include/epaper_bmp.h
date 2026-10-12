#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define EPAPER_FRAME_BYTES 192000
typedef struct { uint32_t width, height, offset, stride; } epaper_bmp_info_t;
bool epaper_bmp_inspect(FILE *file, int rotation, epaper_bmp_info_t *out);
bool epaper_bmp_decode(FILE *file, int rotation, uint8_t frame[EPAPER_FRAME_BYTES], uint32_t *hash);
uint32_t epaper_hash(const void *data, size_t length, uint32_t initial);
int epaper_natural_compare(const char *a, const char *b);

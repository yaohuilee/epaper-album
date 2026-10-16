#pragma once
#include "epaper_image.h"
#include <stdio.h>

typedef struct {
    void (*lock)(void);
    void (*unlock)(void);
    void (*report)(unsigned width,unsigned height,unsigned progress,size_t workspace);
} epaper_png_hooks_t;
bool epaper_png_decode(FILE *file,epaper_image_map_t *map,unsigned width,unsigned height,
    uint8_t **rgb,const epaper_png_hooks_t *hooks,char *error,size_t error_size);

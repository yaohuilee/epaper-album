#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define EPAPER_IMAGE_MAX_EDGE 12000u
#define EPAPER_IMAGE_MAX_PIXELS 40000000u
#define EPAPER_IMAGE_MAX_TARGET 800u

typedef struct {uint16_t low,high;uint8_t fraction;} epaper_image_sample_t;
typedef struct {
    unsigned source_width,source_height,width,height,orientation;
    epaper_image_sample_t x[EPAPER_IMAGE_MAX_TARGET],y[EPAPER_IMAGE_MAX_TARGET];
} epaper_image_map_t;
typedef struct {unsigned width,height,stride;uint32_t offset;bool top_down;} epaper_image_bmp_t;
bool epaper_image_bmp_header(const uint8_t header[54],uint64_t bytes,epaper_image_bmp_t *out);
void epaper_image_bmp_output_header(uint8_t header[54],unsigned width,unsigned height);
bool epaper_image_map(epaper_image_map_t *map,unsigned sw,unsigned sh,unsigned width,unsigned height,unsigned orientation);
void epaper_image_original(const epaper_image_map_t *map,unsigned x,unsigned y,unsigned *sx,unsigned *sy);
void epaper_image_oriented(const epaper_image_map_t *map,unsigned sx,unsigned sy,unsigned *x,unsigned *y);
void epaper_image_weights(const epaper_image_map_t *map,unsigned x,unsigned y,unsigned weights[4]);
unsigned epaper_image_first(const epaper_image_sample_t *map,unsigned count,unsigned edge);
void epaper_image_tone(const uint8_t *rgb,size_t pixels,uint8_t lut[256]);
void epaper_image_dither_row(const uint8_t *rgb,unsigned width,unsigned y,const uint8_t lut[256],int32_t *current,int32_t *next,uint8_t *bgr);
unsigned epaper_image_exif_orientation(const uint8_t *segment,size_t bytes);

void epaper_image_add_block(const epaper_image_map_t *map,uint16_t *accum,const uint8_t *bitmap,unsigned left,unsigned right,unsigned top,unsigned bottom);

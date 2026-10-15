#pragma once
#include "esp_err.h"
#include "epaper_album.h"
#include "cJSON.h"
bool epaper_image_active(void);
bool epaper_image_upload_name(const char *name,char *output,size_t size);
/* Called while the HTTP handler owns the SD/file lock. */
esp_err_t epaper_image_receive(const char *album,const char *source_name,const char *output_path,int rotation,char *source_path,size_t size,unsigned *job,char *error,size_t error_size);
void epaper_image_received(size_t bytes);
esp_err_t epaper_image_submit(void);
void epaper_image_abort(const char *message);
void epaper_image_json(cJSON *root);

void epaper_image_cleanup(void);

#pragma once
/* Create a new album directory or remove an empty configured album. */
#include "esp_err.h"
#include "epaper_album.h"
esp_err_t epaper_album_start(bool deep_wake);
bool epaper_album_due(void);
bool epaper_album_can_sleep(void);
uint64_t epaper_album_sleep_us(void);
void epaper_album_prepare_sleep(void);
const epaper_time_config_t *epaper_album_time_config(void);
void epaper_album_clock_adjust(int64_t delta_us);
esp_err_t epaper_display_frame(const uint8_t *frame);
typedef struct {
    bool ready,enabled,automatic_stopped,night_paused;
    unsigned albums,photos;
    char current_album[EPAPER_ID_MAX],current_photo[EPAPER_PATH_MAX];
    uint32_t interval;
    uint64_t next_us;
} epaper_album_status_t;
/* Snapshot on the application task, never access the player from HTTP handlers. */
void epaper_album_status(epaper_album_status_t *out);
const epaper_album_config_t *epaper_album_configuration(void);
esp_err_t epaper_album_settings(const char *request,char *error,size_t size);
esp_err_t epaper_album_manage(const char *request,bool deleting,char *error,size_t size);
esp_err_t epaper_album_time_settings(const char *request,char *error,size_t size);
esp_err_t epaper_album_wifi_settings(const char *request,char *error,size_t size);
/* Reindex file mutations; preserve still-valid display identity and deadline. */
esp_err_t epaper_album_rescan(void);
esp_err_t epaper_album_mark_display_dirty(void);
esp_err_t epaper_album_restore_current(void);
/* Restore exact pre-provisioning root JSON, including initially disabled Wi-Fi. */
esp_err_t epaper_album_wifi_restore_root(const char *json);
bool epaper_album_root_json(char **output);
esp_err_t epaper_album_save_root_json(const char *json,char *error,size_t size);

esp_err_t epaper_album_rename(const char *request,char *error,size_t size);

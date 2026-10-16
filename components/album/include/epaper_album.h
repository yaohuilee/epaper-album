#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "epaper_bmp.h"
#define EPAPER_MAX_ALBUMS 64
#define EPAPER_MAX_PHOTOS 2048
#define EPAPER_PATH_MAX 512
#define EPAPER_ID_MAX 64
typedef enum { ALBUM_BY_ALBUM, ALBUM_ROUND_ROBIN } epaper_strategy_t;
typedef struct {
    bool enabled;
    char ssid[33],password[65],servers[3][128];
    unsigned server_count;
    uint32_t interval,retry,connect_timeout,sync_timeout;
} epaper_time_config_t;
typedef struct {
    char id[EPAPER_ID_MAX],directory[256];
    bool enabled;
    uint32_t interval;
} epaper_album_definition_t;
typedef struct {
    char root[256],manifest[128];
    bool enabled,shuffle,resume,sleep;
    bool night_enabled;
    uint32_t night_start,night_end; /* Beijing minutes after midnight. */
    int rotation;
    epaper_strategy_t strategy;
    epaper_time_config_t time;
    unsigned album_count;
    epaper_album_definition_t albums[EPAPER_MAX_ALBUMS];
} epaper_album_config_t;
typedef struct { char name[256]; bool bad; } epaper_photo_t;
typedef struct {
    epaper_album_definition_t definition;
    size_t count;
    epaper_photo_t *photos;
    uint16_t *deck;
} epaper_index_t;
typedef struct {
    epaper_album_config_t config;
    uint32_t fingerprint;
    size_t total;
    epaper_index_t albums[EPAPER_MAX_ALBUMS];
} epaper_catalog_t;
typedef struct {
    uint32_t magic,fingerprint;
    uint32_t seeds[EPAPER_MAX_ALBUMS],cursor[EPAPER_MAX_ALBUMS],previous[EPAPER_MAX_ALBUMS];
    uint32_t active,current_album,current_photo,image_hash;
    char current_id[EPAPER_ID_MAX],current_path[EPAPER_PATH_MAX];
} epaper_player_t;
#define EPAPER_PLAYER_MAGIC 0x414c4232u

bool epaper_album_parse(const char *global_json,const char *manifest_json,
    epaper_album_config_t *out,char *error,size_t error_size);
uint64_t epaper_album_night_wait_us(const epaper_album_config_t *config,uint64_t utc_us);
bool epaper_album_location(const char *global_json,char *root,size_t root_size,
    char *manifest,size_t manifest_size,char *error,size_t error_size);
bool epaper_json_read(const char *path,char **out);
/* Validate a bounded web edit and preserve unrelated manifest fields. */
bool epaper_album_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *error,size_t error_size);
bool epaper_album_manage_edit(const char *global,const char *manifest,const char *request,
    bool deleting,char **output,epaper_album_config_t *config,char *directory,size_t directory_size,
    char *error,size_t error_size);
bool epaper_time_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *error,size_t error_size);
bool epaper_wifi_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *error,size_t error_size);
bool epaper_json_recover(const char *path);
bool epaper_json_replace(const char *path,const char *text);
bool epaper_photo_name_valid(const char *name);
bool epaper_album_file_path(const char *mount,const epaper_album_config_t *config,
    const char *album_id,const char *name,char *out,size_t size);
typedef struct {char name[256];uint64_t bytes;bool header_valid;} epaper_file_entry_t;
typedef enum {FILE_LIST_OK,FILE_LIST_INVALID,FILE_LIST_NOT_FOUND,FILE_LIST_LIMIT,FILE_LIST_IO,FILE_LIST_MEMORY} epaper_file_result_t;
epaper_file_result_t epaper_files_list(const char *mount,const epaper_album_config_t *config,
    const char *album_id,epaper_file_entry_t **out,size_t *count);
bool epaper_photo_validate(const char *path,int rotation);
bool epaper_catalog_scan(const char *mount,const epaper_album_config_t *config,
    epaper_catalog_t *out,char *error,size_t error_size);
void epaper_catalog_free(epaper_catalog_t *catalog);
bool epaper_photo_path(const char *mount,const epaper_catalog_t *catalog,
    unsigned album,unsigned photo,char *out,size_t size);
void epaper_player_init(epaper_catalog_t *catalog,epaper_player_t *player,uint32_t seed);
bool epaper_player_restore(epaper_catalog_t *catalog,epaper_player_t *player,const epaper_player_t *saved);
bool epaper_player_pick(epaper_catalog_t *catalog,epaper_player_t *trial,unsigned *album,unsigned *photo);
bool epaper_player_select(epaper_catalog_t *catalog,epaper_player_t *trial,unsigned album,unsigned photo);
void epaper_player_commit(epaper_catalog_t *catalog,epaper_player_t *trial,unsigned album,unsigned photo,uint32_t hash);
bool epaper_player_valid(const epaper_catalog_t *catalog,const epaper_player_t *player);

bool epaper_album_directory_valid(const char *name);
bool epaper_album_rename_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *old_directory,char *new_directory,size_t directory_size,
    char *error,size_t error_size);

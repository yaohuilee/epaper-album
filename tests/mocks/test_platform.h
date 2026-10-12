#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_INVALID_RESPONSE 3
#define ESP_ERR_NOT_FOUND 4
#define ESP_ERR_NOT_SUPPORTED 5
#define ESP_ERR_INVALID_ARG 6
#define NVS_READWRITE 1
#define RTC_DATA_ATTR
#define CONFIG_EPAPER_CONFIG_PATH "/config.json"
#define EPAPER_SD_MOUNT ".local/test-sd"
#define ESP_LOGI(tag,...) do {(void)(tag);} while(0)
#define ESP_LOGW(tag,...) do {(void)(tag);} while(0)
#define ESP_LOGE(tag,...) do {(void)(tag);} while(0)
const char *esp_err_to_name(esp_err_t err);
uint32_t esp_random(void);
esp_err_t nvs_flash_init_partition(const char *part);
esp_err_t nvs_open_from_partition(const char *part,const char *name,int mode,nvs_handle_t *out);
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size);
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *out);
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);

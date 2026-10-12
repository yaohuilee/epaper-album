#include "epaper_time_sync.h"
#include "epaper_storage.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if CONFIG_EPAPER_PROVISION_WIFI
#include "wifi_credentials.h"
#endif

esp_err_t epaper_wifi_provision(void)
{
#if !CONFIG_EPAPER_PROVISION_WIFI
    return ESP_OK;
#else
    const char *path=EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH;
    char backup[512],temporary[512];
    snprintf(backup,sizeof(backup),"%s.pre-wifi",path);
    snprintf(temporary,sizeof(temporary),"%s.wifi-tmp",path);
    /* Recover the rename window if power failed before the replacement was installed. */
    if(access(path,F_OK) && !access(backup,F_OK) && rename(backup,path)) return ESP_FAIL;
    if(!epaper_json_recover(path)) return ESP_FAIL;
    esp_err_t err=nvs_flash_init_partition("album_state");
    if(err!=ESP_OK) return err;
    nvs_handle_t store;
    err=nvs_open_from_partition("album_state","provision",NVS_READWRITE,&store);
    if(err!=ESP_OK) return err;
    uint32_t version=epaper_hash(EPAPER_PRIVATE_WIFI_JSON,strlen(EPAPER_PRIVATE_WIFI_JSON),2166136261u),saved=0;
    if(nvs_get_u32(store,"wifi",&saved)==ESP_OK && saved==version) {nvs_close(store);return ESP_OK;}
    char *text=NULL,*output=NULL,*manifest_text=NULL,root[256],manifest[128],error[128];
    epaper_album_config_t *validated=NULL;
    cJSON *global=NULL,*private=NULL;
    if(!epaper_json_read(path,&text) || !epaper_album_location(text,root,sizeof(root),manifest,sizeof(manifest),error,sizeof(error))) {err=ESP_ERR_INVALID_RESPONSE;goto end;}
    global=cJSON_Parse(text);private=cJSON_Parse(EPAPER_PRIVATE_WIFI_JSON);
    if(!global || !private) {err=ESP_ERR_NO_MEM;goto end;}
    cJSON_DeleteItemFromObjectCaseSensitive(global,"network");
    cJSON_AddItemToObject(global,"network",cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(private,"network"),true));
    cJSON *time=cJSON_GetObjectItemCaseSensitive(global,"time");
    if(!cJSON_IsObject(time)) {cJSON_DeleteItemFromObjectCaseSensitive(global,"time");time=cJSON_AddObjectToObject(global,"time");}
    cJSON_DeleteItemFromObjectCaseSensitive(time,"rtc_storage");cJSON_AddStringToObject(time,"rtc_storage","utc");
    cJSON_DeleteItemFromObjectCaseSensitive(time,"sync");
    cJSON_AddItemToObject(time,"sync",cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(private,"sync"),true));
    output=cJSON_Print(global);
    if(!output || strlen(output)>16384) {err=ESP_ERR_INVALID_SIZE;goto end;}
    char manifest_path[512];
    int length=snprintf(manifest_path,sizeof(manifest_path),"%s%s/%s",EPAPER_SD_MOUNT,root,manifest);
    validated=malloc(sizeof(*validated));
    if(!validated) {err=ESP_ERR_NO_MEM;goto end;}
    if(length<0 || (size_t)length>=sizeof(manifest_path) ||
       !epaper_json_read(manifest_path,&manifest_text) ||
       !epaper_album_parse(output,manifest_text,validated,error,sizeof(error)) || !validated->time.enabled) {
        err=ESP_ERR_INVALID_RESPONSE;goto end;
    }
    if(!strcmp(text,output)) goto mark;
    if(!access(backup,F_OK)) {
        /* Existing backup is kept; after an interrupted install accept only our exact output. */
        cJSON *previous=cJSON_Parse(text);
        bool same=previous && cJSON_Compare(global,previous,true);cJSON_Delete(previous);
        if(same) goto mark;
        err=ESP_ERR_INVALID_STATE;goto end;
    }
    FILE *file=fopen(temporary,"wb");
    if(!file) {err=ESP_FAIL;goto end;}
    bool written=fwrite(output,1,strlen(output),file)==strlen(output);
    if(fflush(file) || fsync(fileno(file))) written=false;
    if(fclose(file)) written=false;
    if(!written) {err=ESP_FAIL;goto end;}
    if(rename(path,backup)) {err=ESP_FAIL;goto end;}
    if(rename(temporary,path)) {rename(backup,path);err=ESP_FAIL;goto end;}
mark:
    err=nvs_set_u32(store,"wifi",version);
    if(err==ESP_OK) err=nvs_commit(store);
    if(err==ESP_OK) ESP_LOGI("wifi_provision","Private Wi-Fi settings installed in SD config; albums preserved");
end:
    free(text);free(output);free(manifest_text);free(validated);
    cJSON_Delete(global);cJSON_Delete(private);nvs_close(store);return err;
#endif
}

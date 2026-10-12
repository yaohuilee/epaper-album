#include "epaper_runtime.h"
#include "epaper_services.h"
#include "epaper_storage.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define make_directory(path) _mkdir(path)
#define remove_directory(path) _rmdir(path)
#else
#include <unistd.h>
#define make_directory(path) mkdir(path,0775)
#define remove_directory(path) rmdir(path)
#endif

static const char *TAG="album";
static epaper_catalog_t *catalog;
static epaper_player_t live;
static nvs_handle_t store;
static uint8_t *frame;
static bool initialized,dirty,automatic_stopped;
static uint64_t deadline,retry_at;
static RTC_DATA_ATTR struct {uint32_t magic,fingerprint,image_hash;uint64_t deadline;} retained;
static uint64_t now_us(void)
{ struct timeval t;gettimeofday(&t,NULL);return (uint64_t)t.tv_sec*1000000+(uint64_t)t.tv_usec; }
static uint64_t full_interval(void)
{ return (uint64_t)catalog->albums[live.current_album].definition.interval*1000000; }

static esp_err_t persist(void)
{
    epaper_player_t old;size_t size=sizeof(old);
    if(!dirty && nvs_get_blob(store,"player",&old,&size)==ESP_OK && size==sizeof(old) &&
       !memcmp(&old,&live,size)) return ESP_OK;
    esp_err_t err=nvs_set_blob(store,"player",&live,sizeof(live));
    if(err==ESP_OK) err=nvs_set_u8(store,"dirty",0);
    if(err==ESP_OK) err=nvs_commit(store);
    if(err==ESP_OK) dirty=false;
    else ESP_LOGE(TAG,"State persistence failed: %s",esp_err_to_name(err));
    return err;
}
static bool cache_load(char **global,char **manifest)
{
    size_t size=0;
    if(nvs_get_blob(store,"config",NULL,&size)!=ESP_OK || size<10 || size>32778) return false;
    uint8_t *data=malloc(size);if(!data) return false;
    bool ok=false;uint32_t lengths[2];
    if(nvs_get_blob(store,"config",data,&size)!=ESP_OK) goto finish;
    memcpy(lengths,data,8);
    if(!lengths[0] || !lengths[1] || lengths[0]>16385 || lengths[1]>16385 ||
       (size_t)lengths[0]+lengths[1]+8!=size || data[7+lengths[0]] || data[size-1] ||
       memchr(data+8,0,lengths[0]-1) || memchr(data+8+lengths[0],0,lengths[1]-1)) goto finish;
    *global=malloc(lengths[0]);*manifest=malloc(lengths[1]);
    if(!*global || !*manifest) {free(*global);free(*manifest);*global=NULL;*manifest=NULL;goto finish;}
    memcpy(*global,data+8,lengths[0]);memcpy(*manifest,data+8+lengths[0],lengths[1]);ok=true;
finish:
    free(data);return ok;
}
static void cache_save(const char *global,const char *manifest)
{
    uint32_t lengths[2]={(uint32_t)strlen(global)+1,(uint32_t)strlen(manifest)+1};
    size_t size=8+(size_t)lengths[0]+lengths[1],old_size=0;
    uint8_t *data=malloc(size);if(!data) return;
    memcpy(data,lengths,8);memcpy(data+8,global,lengths[0]);memcpy(data+8+lengths[0],manifest,lengths[1]);
    bool equal=false;
    if(nvs_get_blob(store,"config",NULL,&old_size)==ESP_OK && old_size==size) {
        uint8_t *old=malloc(size);
        if(old && nvs_get_blob(store,"config",old,&old_size)==ESP_OK) equal=!memcmp(old,data,size);
        free(old);
    }
    if(!equal) {
        esp_err_t err=nvs_set_blob(store,"config",data,size);
        if(err==ESP_OK) err=nvs_commit(store);
        if(err!=ESP_OK) ESP_LOGW(TAG,"Cannot cache validated config: %s",esp_err_to_name(err));
    }
    free(data);
}
static bool load_config(epaper_album_config_t *config)
{
    char *global=NULL,*manifest=NULL,error[160]={0},root[256],name[128],path[EPAPER_PATH_MAX];
    bool ok=false;
    int n=snprintf(path,sizeof(path),"%s%s",EPAPER_SD_MOUNT,CONFIG_EPAPER_CONFIG_PATH);
    if(n>0 && (size_t)n<sizeof(path) && epaper_json_recover(path) && epaper_json_read(path,&global) &&
       epaper_album_location(global,root,sizeof(root),name,sizeof(name),error,sizeof(error))) {
        n=snprintf(path,sizeof(path),"%s%s%s%s",EPAPER_SD_MOUNT,root,!strcmp(root,"/")?"":"/",name);
        if(n>0 && (size_t)n<sizeof(path) && epaper_json_recover(path) && epaper_json_read(path,&manifest))
            ok=epaper_album_parse(global,manifest,config,error,sizeof(error));
    }
    if(ok) {if(error[0]) ESP_LOGW(TAG,"%s",error);cache_save(global,manifest);}
    else {
        ESP_LOGW(TAG,"SD config rejected: %s; trying last valid config",error[0]?error:"missing or unreadable file");
        free(global);free(manifest);global=NULL;manifest=NULL;
        if(cache_load(&global,&manifest)) ok=epaper_album_parse(global,manifest,config,error,sizeof(error));
        if(ok) ESP_LOGW(TAG,"Using cached last valid configuration");
    }
    free(global);free(manifest);return ok;
}
static bool decode(unsigned album,unsigned photo,uint32_t *hash)
{
    char path[EPAPER_PATH_MAX];
    if(!epaper_photo_path(EPAPER_SD_MOUNT,catalog,album,photo,path,sizeof(path))) return false;
    FILE *f=fopen(path,"rb");if(!f) return false;
    bool ok=epaper_bmp_decode(f,catalog->config.rotation,frame,hash);fclose(f);
    if(!ok) ESP_LOGW(TAG,"Skipping unreadable or invalid six-color BMP: %s",path);
    return ok;
}
static esp_err_t show(epaper_player_t *trial,unsigned album,unsigned photo,uint32_t hash,bool force)
{
    if(force || dirty || live.current_album==UINT32_MAX || hash!=live.image_hash) {
        esp_err_t err=nvs_set_u8(store,"dirty",1);
        if(err==ESP_OK) err=nvs_commit(store);
        if(err!=ESP_OK) return err;
        dirty=true;
        err=epaper_display_frame(frame);
        if(err!=ESP_OK) {ESP_LOGE(TAG,"Refresh failed: %s",esp_err_to_name(err));return err;}
    } else ESP_LOGI(TAG,"Preserving identical retained pixels; no refresh");
    epaper_player_commit(catalog,trial,album,photo,hash);live=*trial;
    esp_err_t err=persist();
    deadline=now_us()+full_interval();retry_at=0;automatic_stopped=false;
    ESP_LOGI(TAG,"DISPLAYED album=%s file=%s interval=%"PRIu32" next_in=%.1f seconds",
        live.current_id,live.current_path,catalog->albums[album].definition.interval,full_interval()/1000000.0);
    return err;
}

esp_err_t epaper_album_start(bool deep_wake)
{
    if(initialized) return ESP_ERR_INVALID_STATE;
    esp_err_t err=nvs_flash_init_partition("album_state");
    if(err!=ESP_OK) {
        ESP_LOGE(TAG,"Dedicated album_state NVS needs explicit provisioning: %s; not auto-erased",esp_err_to_name(err));
        return err;
    }
    err=nvs_open_from_partition("album_state","album",NVS_READWRITE,&store);
    if(err!=ESP_OK) return err;
    epaper_album_config_t *config=calloc(1,sizeof(*config));catalog=calloc(1,sizeof(*catalog));
    if(!config || !catalog) {free(config);return ESP_ERR_NO_MEM;}
    if(!load_config(config)) {free(config);return ESP_ERR_INVALID_RESPONSE;}
    char error[160];
    if(!epaper_catalog_scan(EPAPER_SD_MOUNT,config,catalog,error,sizeof(error))) {
        ESP_LOGE(TAG,"%s",error);free(config);return ESP_FAIL;
    }
    free(config);
    ESP_LOGI(TAG,"Indexed %u albums and %u header-valid BMPs; strategy=%s shuffle=%d",
        catalog->config.album_count,(unsigned)catalog->total,
        catalog->config.strategy==ALBUM_BY_ALBUM?"album_by_album":"round_robin",catalog->config.shuffle);
    frame=malloc(EPAPER_FRAME_BYTES);if(!frame) return ESP_ERR_NO_MEM;
    epaper_player_init(catalog,&live,esp_random());
    epaper_player_t saved;size_t saved_size=sizeof(saved);
    bool has_saved=nvs_get_blob(store,"player",&saved,&saved_size)==ESP_OK && saved_size==sizeof(saved);
    uint8_t dirty_value=1;
    nvs_get_u8(store,"dirty",&dirty_value);dirty=dirty_value!=0;
    initialized=true;
    bool restored=(catalog->config.resume || deep_wake) && has_saved && epaper_player_restore(catalog,&live,&saved);
    if(restored) {
        uint32_t hash;
        if(decode(live.current_album,live.current_photo,&hash)) {
            epaper_player_t trial=live;
            bool pixels_preserved=!dirty && hash==live.image_hash;
            err=show(&trial,live.current_album,live.current_photo,hash,false);
            if(err==ESP_OK && pixels_preserved && deep_wake && retained.magic==EPAPER_PLAYER_MAGIC &&
               retained.fingerprint==catalog->fingerprint && retained.image_hash==hash) {
                deadline=retained.deadline;
                ESP_LOGI(TAG,"Deep wake: retained original deadline, due=%d",epaper_album_due());
            } else ESP_LOGI(TAG,"Cold/recovery start: new full interval, no catch-up");
            return err;
        }
        catalog->albums[live.current_album].photos[live.current_photo].bad=true;
        epaper_player_init(catalog,&live,esp_random());
    }
    return epaper_album_next();
}

esp_err_t epaper_album_next(void)
{
    if(!initialized) return ESP_ERR_INVALID_STATE;
    epaper_player_t trial=live;
    for(size_t attempt=0;attempt<catalog->total;attempt++) {
        unsigned album,photo;
        if(!epaper_player_pick(catalog,&trial,&album,&photo)) break;
        uint32_t hash;
        if(!decode(album,photo,&hash)) {catalog->albums[album].photos[photo].bad=true;continue;}
        esp_err_t err=show(&trial,album,photo,hash,false);
        if(err!=ESP_OK) retry_at=now_us()+60000000;
        return err;
    }
    automatic_stopped=true;
    ESP_LOGW(TAG,"No valid photos remain; keeping the existing screen and stopping automatic refresh");
    return ESP_ERR_NOT_FOUND;
}
esp_err_t epaper_display_bmp(const char *path)
{ (void)path;return ESP_ERR_NOT_SUPPORTED; /* Prevent bypassing persistent album state. */ }
bool epaper_album_due(void)
{
    return initialized && !automatic_stopped && catalog->config.enabled &&
        live.current_album!=UINT32_MAX && now_us()>=deadline && now_us()>=retry_at;
}
bool epaper_album_can_sleep(void)
{ return initialized && catalog->config.sleep; }
uint64_t epaper_album_sleep_us(void)
{
    if(!initialized || automatic_stopped || !catalog->config.enabled) return 0;
    uint64_t now=now_us(),target=deadline>retry_at?deadline:retry_at;
    return target>now?target-now:1000000;
}
void epaper_album_prepare_sleep(void)
{
    retained.magic=EPAPER_PLAYER_MAGIC;retained.fingerprint=catalog->fingerprint;
    retained.image_hash=live.image_hash;retained.deadline=deadline;
    nvs_close(store);
}
const epaper_time_config_t *epaper_album_time_config(void)
{ return initialized?&catalog->config.time:NULL; }
void epaper_album_status(epaper_album_status_t *out)
{
    if(!out) return;
    *out=(epaper_album_status_t){0};
    if(!initialized || !catalog) return;
    out->ready=true;out->enabled=catalog->config.enabled;out->automatic_stopped=automatic_stopped;
    out->albums=catalog->config.album_count;out->photos=(unsigned)catalog->total;
    memcpy(out->current_album,live.current_id,sizeof(out->current_album));
    memcpy(out->current_photo,live.current_path,sizeof(out->current_photo));
    if(live.current_album<catalog->config.album_count) out->interval=catalog->albums[live.current_album].definition.interval;
    out->next_us=epaper_album_sleep_us();
}
void epaper_album_clock_adjust(int64_t delta_us)
{
    /* A network time step must preserve the remaining album interval. */
    if(deadline) deadline=(uint64_t)((int64_t)deadline+delta_us);
    if(retry_at) retry_at=(uint64_t)((int64_t)retry_at+delta_us);
    if(retained.magic==EPAPER_PLAYER_MAGIC && retained.deadline)
        retained.deadline=(uint64_t)((int64_t)retained.deadline+delta_us);
}

const epaper_album_config_t *epaper_album_configuration(void)
{return initialized?&catalog->config:NULL;}
esp_err_t epaper_album_rescan(void)
{
    if(!initialized) return ESP_ERR_INVALID_STATE;
    epaper_catalog_t *replacement=calloc(1,sizeof(*replacement));
    if(!replacement) return ESP_ERR_NO_MEM;
    char error[160];
    if(!epaper_catalog_scan(EPAPER_SD_MOUNT,&catalog->config,replacement,error,sizeof(error))) {
        free(replacement);return ESP_FAIL;
    }
    epaper_player_t trial;epaper_player_init(replacement,&trial,esp_random());
    bool restored=epaper_player_restore(replacement,&trial,&live);
    epaper_catalog_t *old=catalog;catalog=replacement;
    epaper_catalog_free(old);free(old);
    if(!catalog->total) {
        /* Keep the displayed identity for status; there is nothing to advance to. */
        live.current_album=live.current_photo=UINT32_MAX;automatic_stopped=true;retry_at=0;
        ESP_LOGI(TAG,"File index empty; keeping current pixels and stopping automatic playback");
        return ESP_OK;
    }
    live=trial;automatic_stopped=false;retry_at=0;
    if(restored) {
        /* Do not clear a dirty-panel marker just because files were reindexed. */
        ESP_LOGI(TAG,"File index refreshed; current photo and deadline preserved");
        return dirty?ESP_OK:persist();
    }
    return epaper_album_next();
}
static esp_err_t root_settings(const char *request,char *error,size_t size,bool wifi)
{
    if(!initialized) return ESP_ERR_INVALID_STATE;
    char *global=NULL,*manifest=NULL,*output=NULL;char path[EPAPER_PATH_MAX];
    epaper_album_config_t *config=malloc(sizeof(*config));esp_err_t err=ESP_FAIL;
    if(!config) {err=ESP_ERR_NO_MEM;goto end;}
    if(!epaper_json_recover(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH) ||
       !epaper_json_read(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,&global)) goto end;
    int n=snprintf(path,sizeof(path),"%s%s%s%s",EPAPER_SD_MOUNT,catalog->config.root,
        !strcmp(catalog->config.root,"/")?"":"/",catalog->config.manifest);
    if(n<0 || (size_t)n>=sizeof(path) || !epaper_json_recover(path) || !epaper_json_read(path,&manifest)) goto end;
    if(!epaper_album_parse(global,manifest,config,error,size) || memcmp(config,&catalog->config,sizeof(*config))) {
        snprintf(error,size,"SD configuration changed externally; restart first");goto end;
    }
    bool edited=wifi?epaper_wifi_edit(global,manifest,request,&output,config,error,size):
        epaper_time_edit(global,manifest,request,&output,config,error,size);
    if(!edited) {err=ESP_ERR_INVALID_RESPONSE;goto end;}
    if(!epaper_json_replace(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,output)) {
        snprintf(error,size,"Cannot save SD configuration; settings unchanged");goto end;
    }
    /* Only time changes: preserve catalog, player, image and all album deadlines. */
    catalog->config.time=config->time;cache_save(output,manifest);err=ESP_OK;
end:
    free(global);free(manifest);free(output);free(config);
    if(err!=ESP_OK && error && size && !error[0]) snprintf(error,size,"Settings not applied: %s",esp_err_to_name(err));
    return err;
}
esp_err_t epaper_album_time_settings(const char *request,char *error,size_t size)
{return root_settings(request,error,size,false);}
esp_err_t epaper_album_wifi_settings(const char *request,char *error,size_t size)
{return root_settings(request,error,size,true);}

esp_err_t epaper_album_settings(const char *request,char *error,size_t size)
{
    if(!initialized) return ESP_ERR_INVALID_STATE;
    char *global=NULL,*manifest=NULL,*output=NULL;
    char root[256],name[128],path[EPAPER_PATH_MAX];
    epaper_album_config_t *config=malloc(sizeof(*config));
    epaper_catalog_t *replacement=calloc(1,sizeof(*replacement));
    esp_err_t err=ESP_FAIL;
    if(!config || !replacement) {err=ESP_ERR_NO_MEM;goto end;}
    if(!epaper_json_read(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,&global) ||
       !epaper_album_location(global,root,sizeof(root),name,sizeof(name),error,size)) goto end;
    int n=snprintf(path,sizeof(path),"%s%s%s%s",EPAPER_SD_MOUNT,root,!strcmp(root,"/")?"":"/",name);
    if(n<0 || (size_t)n>=sizeof(path) || !epaper_json_recover(path) || !epaper_json_read(path,&manifest)) goto end;
    if(!epaper_album_edit(global,manifest,request,&output,config,error,size)) {err=ESP_ERR_INVALID_RESPONSE;goto end;}
    /* Web editing must not change the live hardware/network configuration. */
    if(strcmp(config->root,catalog->config.root) || strcmp(config->manifest,catalog->config.manifest) ||
       config->rotation!=catalog->config.rotation || memcmp(&config->time,&catalog->config.time,sizeof(config->time))) {
        snprintf(error,size,"SD configuration changed externally; restart first");goto end;
    }
    if(!epaper_catalog_scan(EPAPER_SD_MOUNT,config,replacement,error,size)) goto end;
    if(!replacement->total) {snprintf(error,size,"Selected albums contain no valid photos");goto end;}
    /* Validate and build the replacement before touching disk or live playback. */
    if(!epaper_json_replace(path,output)) {snprintf(error,size,"Cannot save SD configuration; playback unchanged");goto end;}
    cache_save(global,output);
    epaper_player_t trial;epaper_player_init(replacement,&trial,esp_random());
    bool restored=epaper_player_restore(replacement,&trial,&live);
    epaper_catalog_t *old=catalog;catalog=replacement;replacement=old;
    live=trial;automatic_stopped=false;retry_at=0;
    if(restored) {deadline=now_us()+full_interval();err=persist();}
    else err=epaper_album_next();
    if(err!=ESP_OK) snprintf(error,size,"Settings saved; display/state update failed: %s",esp_err_to_name(err));
    else ESP_LOGI(TAG,"Web settings saved to SD; selected albums applied");
end:
    free(global);free(manifest);free(output);free(config);
    epaper_catalog_free(replacement);free(replacement);
    if(err!=ESP_OK && error && size && !error[0]) snprintf(error,size,"Settings not applied: %s",esp_err_to_name(err));
    return err;
}

esp_err_t epaper_album_manage(const char *request,bool deleting,char *error,size_t size)
{
    if(!initialized) return ESP_ERR_INVALID_STATE;
    char *global=NULL,*manifest=NULL,*output=NULL;
    char manifest_path[EPAPER_PATH_MAX],directory[256],path[EPAPER_PATH_MAX];
    epaper_album_config_t *config=malloc(sizeof(*config));
    epaper_catalog_t *replacement=calloc(1,sizeof(*replacement));
    bool directory_changed=false,saved=false;esp_err_t err=ESP_FAIL;
    if(!config || !replacement) {err=ESP_ERR_NO_MEM;goto end;}
    int n=snprintf(manifest_path,sizeof(manifest_path),"%s%s%s%s",EPAPER_SD_MOUNT,catalog->config.root,
        !strcmp(catalog->config.root,"/")?"":"/",catalog->config.manifest);
    if(n<0 || (size_t)n>=sizeof(manifest_path) ||
       !epaper_json_recover(manifest_path) || !epaper_json_read(manifest_path,&manifest) ||
       !epaper_json_read(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,&global)) goto end;
    if(!epaper_album_parse(global,manifest,config,error,size) || memcmp(config,&catalog->config,sizeof(*config))) {
        snprintf(error,size,"SD configuration changed externally; restart first");goto end;
    }
    if(!epaper_album_manage_edit(global,manifest,request,deleting,&output,config,directory,sizeof(directory),error,size)) {
        err=ESP_ERR_INVALID_ARG;goto end;
    }
    n=snprintf(path,sizeof(path),"%s%s%s%s",EPAPER_SD_MOUNT,config->root,!strcmp(config->root,"/")?"":"/",directory);
    if(n<0 || (size_t)n>=sizeof(path)) goto end;
    if(deleting) {
        /* rmdir removes empty directories only, including non-BMP/hidden entries. */
        if(remove_directory(path)) {
            if(errno!=ENOENT) {snprintf(error,size,"Album directory must be empty; delete photos first");goto end;}
        } else directory_changed=true;
    } else {
        if(make_directory(path)) {snprintf(error,size,"Cannot create album directory; existing directories are not reused");goto end;}
        directory_changed=true;
    }
    if(!epaper_catalog_scan(EPAPER_SD_MOUNT,config,replacement,error,size)) goto end;
    if(!epaper_json_replace(manifest_path,output)) {snprintf(error,size,"Cannot save album manifest");goto end;}
    saved=true;cache_save(global,output);
    epaper_player_t trial;epaper_player_init(replacement,&trial,esp_random());
    bool restored=epaper_player_restore(replacement,&trial,&live);
    epaper_catalog_t *old=catalog;catalog=replacement;replacement=old;
    if(!catalog->total) {
        live.current_album=live.current_photo=UINT32_MAX;automatic_stopped=true;retry_at=0;err=ESP_OK;
    } else {
        live=trial;automatic_stopped=false;retry_at=0;
        err=restored?(dirty?ESP_OK:persist()):epaper_album_next();
    }
    if(err!=ESP_OK) snprintf(error,size,"Album manifest saved; display/state update failed: %s",esp_err_to_name(err));
end:
    if(directory_changed && !saved) {
        int rollback=deleting?make_directory(path):remove_directory(path);
        if(rollback) snprintf(error,size,"Manifest unchanged; directory rollback failed");
    }
    free(global);free(manifest);free(output);free(config);
    epaper_catalog_free(replacement);free(replacement);
    if(err!=ESP_OK && error && size && !error[0]) snprintf(error,size,"Album operation failed: %s",esp_err_to_name(err));
    return err;
}

esp_err_t epaper_album_mark_display_dirty(void)
{
    dirty=true;
    if(!initialized) return ESP_OK;
    esp_err_t err=nvs_set_u8(store,"dirty",1);return err==ESP_OK?nvs_commit(store):err;
}
esp_err_t epaper_album_restore_current(void)
{
    if(!initialized) return ESP_ERR_INVALID_STATE;
    uint32_t hash;
    if(live.current_album==UINT32_MAX || !decode(live.current_album,live.current_photo,&hash)) return epaper_album_next();
    epaper_player_t trial=live;
    return show(&trial,live.current_album,live.current_photo,hash,true);
}
esp_err_t epaper_album_wifi_restore_root(const char *json)
{
    if(!initialized || !json) return ESP_ERR_INVALID_STATE;
    char path[EPAPER_PATH_MAX],error[160],*manifest=NULL;
    epaper_album_config_t *config=malloc(sizeof(*config));
    if(!config) return ESP_ERR_NO_MEM;
    int n=snprintf(path,sizeof(path),"%s%s%s%s",EPAPER_SD_MOUNT,catalog->config.root,
        !strcmp(catalog->config.root,"/")?"":"/",catalog->config.manifest);
    esp_err_t err=ESP_FAIL;
    if(n>0 && (size_t)n<sizeof(path) && epaper_json_read(path,&manifest) &&
       epaper_album_parse(json,manifest,config,error,sizeof(error)) &&
       epaper_json_replace(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,json)) {
        catalog->config.time=config->time;cache_save(json,manifest);err=ESP_OK;
    }
    free(manifest);free(config);return err;
}

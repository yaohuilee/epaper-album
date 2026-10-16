#include "epaper_runtime.h"
#include "epaper_services.h"
#include "epaper_storage.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define journal_sync(f) _commit(_fileno(f))
#define make_directory(path) _mkdir(path)
#define remove_directory(path) _rmdir(path)
#else
#include <unistd.h>
#define journal_sync(f) fsync(fileno(f))
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
    for(unsigned a=0;a<catalog->config.album_count;a++)if(!catalog->albums[a].definition.enabled){live.cursor[a]=0;live.previous[a]=UINT32_MAX;}
    epaper_player_t old;size_t size=sizeof(old);
    if(!dirty && nvs_get_blob(store,"player",&old,&size)==ESP_OK && size==sizeof(old) &&
       !memcmp(&old,&live,size)) return ESP_OK;
    esp_err_t err=nvs_set_blob(store,"player",&live,sizeof(live));
    if(err==ESP_OK) err=nvs_set_u8(store,"manual",live.current_album<catalog->config.album_count && !catalog->albums[live.current_album].definition.enabled);
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
static bool album_directory_path(const char *root,const char *directory,char *path,size_t size)
{
    int n=snprintf(path,size,"%s%s%s%s",EPAPER_SD_MOUNT,root,!strcmp(root,"/")?"":"/",directory);
    return n>0 && (size_t)n<size;
}
static bool rename_recover(const epaper_album_config_t *config,const char *manifest_path)
{
    char journal[EPAPER_PATH_MAX+32],old_path[EPAPER_PATH_MAX],new_path[EPAPER_PATH_MAX],*text=NULL;
    snprintf(journal,sizeof(journal),"%s.rename-pending",manifest_path);
    struct stat st;if(stat(journal,&st))return errno==ENOENT;
    if(!epaper_json_read(journal,&text))return false;
    cJSON *doc=cJSON_Parse(text);free(text);bool ok=false;
    const cJSON *id=cJSON_GetObjectItemCaseSensitive(doc,"id"),*old=cJSON_GetObjectItemCaseSensitive(doc,"old"),*next=cJSON_GetObjectItemCaseSensitive(doc,"new");
    if(!cJSON_IsString(id) || !cJSON_IsString(old) || !cJSON_IsString(next) ||
       !epaper_album_directory_valid(old->valuestring) || !epaper_album_directory_valid(next->valuestring) ||
       !album_directory_path(config->root,old->valuestring,old_path,sizeof(old_path)) ||
       !album_directory_path(config->root,next->valuestring,new_path,sizeof(new_path)))goto end;
    const char *chosen=NULL;
    for(unsigned i=0;i<config->album_count;i++)if(!strcmp(config->albums[i].id,id->valuestring))chosen=config->albums[i].directory;
    if(!chosen || (strcmp(chosen,old->valuestring) && strcmp(chosen,next->valuestring)))goto end;
    bool have_old=stat(old_path,&st)==0;if(!have_old && errno!=ENOENT)goto end;if(have_old && (st.st_mode&S_IFMT)!=S_IFDIR)goto end;
    bool have_new=stat(new_path,&st)==0;if(!have_new && errno!=ENOENT)goto end;if(have_new && (st.st_mode&S_IFMT)!=S_IFDIR)goto end;
    if(have_old==have_new)goto end;
    bool old_selected=!strcmp(chosen,old->valuestring);
    if(old_selected!=have_old && rename(have_old?old_path:new_path,old_selected?old_path:new_path))goto end;
    ok=remove(journal)==0;
    if(ok)ESP_LOGI(TAG,"Recovered pending album directory rename");
end:cJSON_Delete(doc);return ok;
}
static bool rename_journal_write(const char *path,const char *id,const char *old,const char *next)
{
    cJSON *doc=cJSON_CreateObject();if(!doc)return false;
    bool ok=cJSON_AddStringToObject(doc,"id",id) && cJSON_AddStringToObject(doc,"old",old) && cJSON_AddStringToObject(doc,"new",next);
    char *text=ok?cJSON_PrintUnformatted(doc):NULL;cJSON_Delete(doc);if(!text)return false;
    char tmp[EPAPER_PATH_MAX+40];snprintf(tmp,sizeof(tmp),"%s.tmp",path);
    FILE *f=fopen(tmp,"wb");if(!f){free(text);return false;}
    size_t n=strlen(text);ok=fwrite(text,1,n,f)==n;free(text);
    if(fflush(f) || journal_sync(f))ok=false;
    if(fclose(f))ok=false;
    if(ok)ok=rename(tmp,path)==0;
    if(!ok)remove(tmp);
    return ok;
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
    if(ok && !rename_recover(config,path)) {
        ESP_LOGE(TAG,"Pending album rename recovery failed; preserve SD files for repair");
        free(global);free(manifest);return false;
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
/* A disabled album may cache one displayed file, without entering its deck in
   automatic playback or increasing the enabled-photo count/fingerprint. */
static esp_err_t cache_manual(epaper_catalog_t *c,unsigned album,const char *name)
{
    char path[EPAPER_PATH_MAX];epaper_bmp_info_t info;
    if(!epaper_album_file_path(EPAPER_SD_MOUNT,&c->config,c->albums[album].definition.id,name,path,sizeof(path)))return ESP_ERR_INVALID_ARG;
    FILE *f=fopen(path,"rb");if(!f)return ESP_ERR_NOT_FOUND;
    bool valid=epaper_bmp_inspect(f,c->config.rotation,&info);fclose(f);
    if(!valid)return ESP_ERR_INVALID_RESPONSE;
    epaper_photo_t *photo=calloc(1,sizeof(*photo));uint16_t *deck=malloc(sizeof(*deck));
    if(!photo || !deck){free(photo);free(deck);return ESP_ERR_NO_MEM;}
    strcpy(photo->name,name);*deck=0;
    epaper_index_t *index=&c->albums[album];free(index->photos);free(index->deck);
    index->photos=photo;index->deck=deck;index->count=1;return ESP_OK;
}
static bool manual_displayed(void)
{return live.current_album<catalog->config.album_count && !catalog->albums[live.current_album].definition.enabled;}
static esp_err_t restore_manual(epaper_catalog_t *c,const epaper_player_t *identity)
{
    if(!identity || identity->magic!=EPAPER_PLAYER_MAGIC ||
       !memchr(identity->current_id,0,sizeof(identity->current_id)) || !memchr(identity->current_path,0,sizeof(identity->current_path)))return ESP_OK;
    const char *name=strrchr(identity->current_path,'/');if(!name || !epaper_photo_name_valid(++name))return ESP_OK;
    for(unsigned a=0;a<c->config.album_count;a++)if(!c->albums[a].definition.enabled && !strcmp(c->albums[a].definition.id,identity->current_id)) {
        char path[EPAPER_PATH_MAX];
        if(!epaper_album_file_path("",&c->config,identity->current_id,name,path,sizeof(path)) || strcmp(path,identity->current_path))return ESP_OK;
        esp_err_t err=cache_manual(c,a,name);return err==ESP_ERR_NO_MEM?err:ESP_OK;
    }
    return ESP_OK;
}
static bool restore_live(epaper_catalog_t *c,epaper_player_t *trial,const epaper_player_t *identity)
{
    if(!epaper_player_restore(c,trial,identity))return false;
    if(!manual_displayed() || c->config.shuffle!=catalog->config.shuffle || c->config.strategy!=catalog->config.strategy)return true;
    /* A one-off photo must not consume the paused queue when disabled folders
       are renamed/added. Match enabled decks by stable ID and sorted filenames. */
    for(unsigned a=0;a<c->config.album_count;a++)if(c->albums[a].definition.enabled) {
        for(unsigned b=0;b<catalog->config.album_count;b++) {
            const epaper_index_t *old=&catalog->albums[b],*next=&c->albums[a];
            if(!old->definition.enabled || strcmp(old->definition.id,next->definition.id) || old->count!=next->count)continue;
            bool same=live.cursor[b]<=old->count && (live.previous[b]==UINT32_MAX || live.previous[b]<old->count);
            for(size_t p=0;p<old->count && same;p++)same=!strcmp(old->photos[p].name,next->photos[p].name);
            if(same){trial->seeds[a]=live.seeds[b];trial->cursor[a]=live.cursor[b];trial->previous[a]=live.previous[b];}
        }
    }
    for(unsigned offset=0;offset<catalog->config.album_count;offset++) {
        unsigned old=(live.active+offset)%catalog->config.album_count;
        for(unsigned a=0;a<c->config.album_count;a++)if(!strcmp(c->albums[a].definition.id,catalog->albums[old].definition.id)) {trial->active=a;return true;}
    }
    return true;
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
    deadline=now_us()+full_interval();retry_at=0;automatic_stopped=!catalog->total;
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
    uint8_t saved_manual=0;nvs_get_u8(store,"manual",&saved_manual);
    if(has_saved && saved_manual && restore_manual(catalog,&saved)!=ESP_OK)return ESP_ERR_NO_MEM;
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
esp_err_t epaper_album_show_photo(const char *request,char *error,size_t size)
{
    if(!initialized)return ESP_ERR_INVALID_STATE;
    cJSON *doc=request?cJSON_ParseWithOpts(request,NULL,true):NULL;unsigned seen=0;bool valid=cJSON_IsObject(doc) && !strstr(request,"\\u0000");
    for(const cJSON *f=doc?doc->child:NULL;f && valid;f=f->next) {
        unsigned bit=!strcmp(f->string,"schema_version")?1:!strcmp(f->string,"album")?2:!strcmp(f->string,"name")?4:0;
        if(!bit || (seen&bit))valid=false;else seen|=bit;
    }
    const cJSON *schema=cJSON_GetObjectItemCaseSensitive(doc,"schema_version"),*album=cJSON_GetObjectItemCaseSensitive(doc,"album"),*name=cJSON_GetObjectItemCaseSensitive(doc,"name");
    valid=valid && seen==7 && cJSON_IsNumber(schema) && schema->valuedouble==1 && cJSON_IsString(album) && cJSON_IsString(name) && epaper_photo_name_valid(name->valuestring);
    if(!valid){snprintf(error,size,"Use schema_version 1, album and a safe BMP name");cJSON_Delete(doc);return ESP_ERR_INVALID_ARG;}
    unsigned a=0;while(a<catalog->config.album_count && strcmp(catalog->albums[a].definition.id,album->valuestring))a++;
    if(a==catalog->config.album_count){snprintf(error,size,"Unknown album");cJSON_Delete(doc);return ESP_ERR_NOT_FOUND;}
    epaper_player_t trial=live;
    if(!epaper_player_valid(catalog,&live)){epaper_player_init(catalog,&trial,esp_random());epaper_player_restore(catalog,&trial,&live);}
    epaper_index_t previous=catalog->albums[a];bool manual=!previous.definition.enabled;unsigned p=0;esp_err_t err=ESP_OK;
    if(manual) {
        catalog->albums[a].photos=NULL;catalog->albums[a].deck=NULL;catalog->albums[a].count=0;
        err=cache_manual(catalog,a,name->valuestring);
    } else {
        while(p<previous.count && strcmp(previous.photos[p].name,name->valuestring))p++;
        if(p==previous.count)err=ESP_ERR_NOT_FOUND;
    }
    uint32_t hash=0;
    if(err==ESP_OK && !decode(a,p,&hash))err=ESP_ERR_INVALID_RESPONSE;
    char path[EPAPER_PATH_MAX]={0};bool applied=false;
    if(err==ESP_OK) {
        epaper_photo_path("",catalog,a,p,path,sizeof(path));
        if(!dirty && !strcmp(live.current_id,album->valuestring) && !strcmp(live.current_path,path) && live.image_hash==hash)applied=true;
        else {
            if(!manual && !epaper_player_select(catalog,&trial,a,p))err=ESP_FAIL;
            else {err=show(&trial,a,p,hash,false);applied=!strcmp(live.current_path,path) && live.image_hash==hash;}
        }
        if(applied)catalog->albums[a].photos[p].bad=false;
    }
    if(manual) {
        if(applied){free(previous.photos);free(previous.deck);}
        else {free(catalog->albums[a].photos);free(catalog->albums[a].deck);catalog->albums[a]=previous;}
    }
    if(err!=ESP_OK)snprintf(error,size,applied?"Photo displayed; state save failed: %s":"Photo not displayed: %s",esp_err_to_name(err));
    cJSON_Delete(doc);return err;
}
esp_err_t epaper_display_bmp(const char *path)
{ (void)path;return ESP_ERR_NOT_SUPPORTED; /* Prevent bypassing persistent album state. */ }
static uint64_t automatic_target(uint64_t now)
{
    uint64_t target=deadline>retry_at?deadline:retry_at;
    uint64_t wait=epaper_album_night_wait_us(&catalog->config,now);
    if(wait && target<now+wait) target=now+wait;
    target+=epaper_album_night_wait_us(&catalog->config,target);
    return target;
}
bool epaper_album_due(void)
{
    return initialized && !automatic_stopped && catalog->config.enabled &&
        live.current_album!=UINT32_MAX && now_us()>=automatic_target(now_us());
}
bool epaper_album_can_sleep(void)
{ return initialized && catalog->config.sleep; }
uint64_t epaper_album_sleep_us(void)
{
    if(!initialized || automatic_stopped || !catalog->config.enabled) return 0;
    uint64_t now=now_us(),target=automatic_target(now);
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
    out->night_paused=epaper_album_night_wait_us(&catalog->config,now_us())>0;
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
    if(manual_displayed() && restore_manual(replacement,&live)!=ESP_OK){epaper_catalog_free(replacement);free(replacement);return ESP_ERR_NO_MEM;}
    epaper_player_t trial;epaper_player_init(replacement,&trial,esp_random());
    bool restored=restore_live(replacement,&trial,&live);
    epaper_catalog_t *old=catalog;catalog=replacement;
    epaper_catalog_free(old);free(old);
    if(!catalog->total) {
        /* Keep the displayed identity for status; there is nothing to advance to. */
        if(restored){live=trial;automatic_stopped=true;retry_at=0;return dirty?ESP_OK:persist();}
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
    bool night_enabled=config->night_enabled;
    uint32_t night_start=config->night_start,night_end=config->night_end;
    cJSON *edit=cJSON_Parse(request);
    if(!edit) {err=ESP_ERR_NO_MEM;goto end;}
    bool selection_request=cJSON_HasObjectItem(edit,"album_ids");
    bool night_request=cJSON_HasObjectItem(edit,"night_pause") && !cJSON_HasObjectItem(edit,"album_ids") && !cJSON_HasObjectItem(edit,"interval_seconds");
    cJSON_Delete(edit);
    config->night_enabled=catalog->config.night_enabled;
    config->night_start=catalog->config.night_start;config->night_end=catalog->config.night_end;
    bool only_night=night_request && !memcmp(config,&catalog->config,sizeof(*config));
    config->night_enabled=night_enabled;config->night_start=night_start;config->night_end=night_end;
    if(only_night) {
        if(!epaper_json_replace(path,output)) {snprintf(error,size,"Cannot save SD configuration; playback unchanged");goto end;}
        catalog->config.night_enabled=config->night_enabled;
        catalog->config.night_start=config->night_start;catalog->config.night_end=config->night_end;
        cache_save(global,output);err=ESP_OK;goto end;
    }
    if(!epaper_catalog_scan(EPAPER_SD_MOUNT,config,replacement,error,size)) goto end;
    if(!replacement->total) {snprintf(error,size,"Selected albums contain no valid photos");goto end;}
    /* Validate and build the replacement before touching disk or live playback. */
    if(!selection_request && manual_displayed() && restore_manual(replacement,&live)!=ESP_OK){err=ESP_ERR_NO_MEM;goto end;}
    if(!epaper_json_replace(path,output)) {snprintf(error,size,"Cannot save SD configuration; playback unchanged");goto end;}
    cache_save(global,output);
    epaper_player_t trial;epaper_player_init(replacement,&trial,esp_random());
    bool restored=restore_live(replacement,&trial,&live);
    epaper_catalog_t *old=catalog;catalog=replacement;replacement=old;
    live=trial;automatic_stopped=false;retry_at=0;
    if(restored) {if(catalog->albums[live.current_album].definition.enabled)deadline=now_us()+full_interval();err=persist();}
    else err=epaper_album_next();
    if(err!=ESP_OK) snprintf(error,size,"Settings saved; display/state update failed: %s",esp_err_to_name(err));
    else ESP_LOGI(TAG,"Web settings saved to SD; selected albums applied");
end:
    free(global);free(manifest);free(output);free(config);
    epaper_catalog_free(replacement);free(replacement);
    if(err!=ESP_OK && error && size && !error[0]) snprintf(error,size,"Settings not applied: %s",esp_err_to_name(err));
    return err;
}

esp_err_t epaper_album_rename(const char *request,char *error,size_t size)
{
    if(!initialized)return ESP_ERR_INVALID_STATE;
    char *global=NULL,*manifest=NULL,*output=NULL;
    char manifest_path[EPAPER_PATH_MAX],old_directory[256],new_directory[256],old_path[EPAPER_PATH_MAX],new_path[EPAPER_PATH_MAX],journal[EPAPER_PATH_MAX+32];
    epaper_album_config_t *config=malloc(sizeof(*config));epaper_catalog_t *replacement=calloc(1,sizeof(*replacement));
    bool moved=false,saved=false,journal_ready=false;esp_err_t err=ESP_FAIL;
    if(!config || !replacement){err=ESP_ERR_NO_MEM;goto end;}
    int n=snprintf(manifest_path,sizeof(manifest_path),"%s%s%s%s",EPAPER_SD_MOUNT,catalog->config.root,!strcmp(catalog->config.root,"/")?"":"/",catalog->config.manifest);
    if(n<=0 || (size_t)n>=sizeof(manifest_path) || !epaper_json_recover(manifest_path) || !epaper_json_read(manifest_path,&manifest) ||
       !epaper_json_read(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,&global))goto end;
    if(!epaper_album_parse(global,manifest,config,error,size) || memcmp(config,&catalog->config,sizeof(*config))) {
        snprintf(error,size,"SD configuration changed externally; restart first");goto end;
    }
    if(!rename_recover(config,manifest_path)){snprintf(error,size,"Pending rename requires recovery");goto end;}
    if(!epaper_album_rename_edit(global,manifest,request,&output,config,old_directory,new_directory,sizeof(old_directory),error,size)){err=ESP_ERR_INVALID_ARG;goto end;}
    if(!album_directory_path(config->root,old_directory,old_path,sizeof(old_path)) || !album_directory_path(config->root,new_directory,new_path,sizeof(new_path)))goto end;
    struct stat st;
    if(!stat(new_path,&st) || errno!=ENOENT){snprintf(error,size,"Target folder already exists or is inaccessible");goto end;}
    const char *id=NULL;for(unsigned i=0;i<config->album_count;i++)if(!strcmp(config->albums[i].directory,new_directory))id=config->albums[i].id;
    snprintf(journal,sizeof(journal),"%s.rename-pending",manifest_path);
    if(!id || !rename_journal_write(journal,id,old_directory,new_directory)){snprintf(error,size,"Cannot save rename recovery journal");goto end;}
    journal_ready=true;
    if(rename(old_path,new_path)){snprintf(error,size,"Cannot rename album folder");goto end;}moved=true;
    if(!epaper_catalog_scan(EPAPER_SD_MOUNT,config,replacement,error,size))goto end;
    /* Same album ID and image bytes: remap the displayed path, retain deadline. */
    epaper_player_t identity=live,trial;
    if(!strcmp(identity.current_id,id)) {
        const char *name=strrchr(identity.current_path,'/');
        if(name){char filename[256];snprintf(filename,sizeof(filename),"%s",name+1);epaper_album_file_path("",config,id,filename,identity.current_path,sizeof(identity.current_path));}
    }
    if(manual_displayed() && restore_manual(replacement,&identity)!=ESP_OK){err=ESP_ERR_NO_MEM;goto end;}
    if(!epaper_json_replace(manifest_path,output)){snprintf(error,size,"Cannot save album manifest");goto end;}saved=true;cache_save(global,output);
    epaper_player_init(replacement,&trial,esp_random());bool restored=restore_live(replacement,&trial,&identity);
    epaper_catalog_t *old_catalog=catalog;catalog=replacement;replacement=old_catalog;live=trial;
    if(!catalog->total){automatic_stopped=true;err=restored&&!dirty?persist():ESP_OK;}
    else {automatic_stopped=false;err=restored?(dirty?ESP_OK:persist()):epaper_album_next();}
    if(err!=ESP_OK)snprintf(error,size,"Folder renamed; display/state update failed: %s",esp_err_to_name(err));
end:
    if(journal_ready) {
        if(moved && !saved && rename(new_path,old_path))snprintf(error,size,"Rename rollback failed; restart to recover");
        else if(remove(journal))ESP_LOGW(TAG,"Rename journal retained; startup will recover");
    }
    free(global);free(manifest);free(output);free(config);epaper_catalog_free(replacement);free(replacement);
    if(err!=ESP_OK && error && size && !error[0])snprintf(error,size,"Album rename failed: %s",esp_err_to_name(err));
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
    if(manual_displayed() && restore_manual(replacement,&live)!=ESP_OK){err=ESP_ERR_NO_MEM;goto end;}
    if(!epaper_json_replace(manifest_path,output)) {snprintf(error,size,"Cannot save album manifest");goto end;}
    saved=true;cache_save(global,output);
    epaper_player_t trial;epaper_player_init(replacement,&trial,esp_random());
    bool restored=restore_live(replacement,&trial,&live);
    epaper_catalog_t *old=catalog;catalog=replacement;replacement=old;
    if(!catalog->total) {
        if(restored)live=trial;else live.current_album=live.current_photo=UINT32_MAX;
        automatic_stopped=true;retry_at=0;err=restored&&!dirty?persist():ESP_OK;
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

bool epaper_album_root_json(char **output)
{
    *output=NULL;
    if(epaper_json_read(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,output))return true;
    char *manifest=NULL;bool ok=initialized && cache_load(output,&manifest);free(manifest);return ok;
}
esp_err_t epaper_album_save_root_json(const char *json,char *error,size_t size)
{
    if(!initialized)return ESP_ERR_INVALID_STATE;
    char path[EPAPER_PATH_MAX],*manifest=NULL;epaper_album_config_t *c=malloc(sizeof(*c));
    if(!c)return ESP_ERR_NO_MEM;
    int n=snprintf(path,sizeof(path),"%s%s%s%s",EPAPER_SD_MOUNT,catalog->config.root,!strcmp(catalog->config.root,"/")?"":"/",catalog->config.manifest);
    esp_err_t err=ESP_FAIL;
    if(n<=0 || (size_t)n>=sizeof(path) || !epaper_json_read(path,&manifest))goto end;
    if(!epaper_album_parse(json,manifest,c,error,size) || memcmp(c,&catalog->config,sizeof(*c))){snprintf(error,size,"Unrelated SD configuration changed; restart first");goto end;}
    if(!epaper_json_replace(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,json)){snprintf(error,size,"Cannot save announcement settings");goto end;}
    cache_save(json,manifest);err=ESP_OK;
end:free(c);free(manifest);return err;
}

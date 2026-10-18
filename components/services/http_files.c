#include "epaper_http_internal.h"
#include "epaper_http.h"
#include "epaper_image_import.h"
#include "epaper_runtime.h"
#include "epaper_storage.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "esp_vfs_fat.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_UPLOAD (2u*1024u*1024u)
#define TRANSFER_US 60000000LL
#define CHUNK_BYTES 8192
#define FILE_BUFFER_BYTES 32768
typedef struct {char album[EPAPER_ID_MAX],name[256];unsigned offset,limit;} query_t;
static esp_err_t json_response(httpd_req_t *req,const char *status,cJSON *doc)
{
    char *text=doc?cJSON_PrintUnformatted(doc):NULL;cJSON_Delete(doc);
    if(!text) return ESP_ERR_NO_MEM;
    httpd_resp_set_status(req,status);httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    esp_err_t err=httpd_resp_send(req,text,HTTPD_RESP_USE_STRLEN);free(text);return err;
}
static esp_err_t error_response(httpd_req_t *req,const char *status,const char *code,const char *message)
{
    cJSON *doc=cJSON_CreateObject();
    cJSON_AddStringToObject(doc,"error",code);cJSON_AddStringToObject(doc,"message",message);
    httpd_resp_set_hdr(req,"Connection","close");
    esp_err_t err=json_response(req,status,doc);
    /* Rejecting an unread upload must not leave its body on a persistent connection. */
    httpd_sess_trigger_close(req->handle,httpd_req_to_sockfd(req));return err;
}
static int hex(char c)
{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static bool decode(const char *in,char *out,size_t size)
{
    size_t n=0;
    for(const char *p=in;*p;p++) {
        unsigned char c=(unsigned char)*p;
        if(c=='%') {
            if(!p[1] || !p[2] || hex(p[1])<0 || hex(p[2])<0) return false;
            c=(unsigned char)((hex(p[1])<<4)|hex(p[2]));p+=2;
        } else if(c=='+') c=' ';
        if(!c || n+1>=size) return false;
        out[n++]=(char)c;
    }
    out[n]=0;return true;
}
static bool integer(const char *s,unsigned max,unsigned *out)
{
    if(!*s) return false;
    unsigned n=0;
    for(const char *p=s;*p;p++) {
        if(*p<'0'||*p>'9'||n>(max-(unsigned)(*p-'0'))/10) return false;
        n=n*10+(unsigned)(*p-'0');if(n>max) return false;
    }
    *out=n;return true;
}
static bool query(httpd_req_t *req,bool photo,query_t *out)
{
    *out=(query_t){.limit=50};size_t length=httpd_req_get_url_query_len(req);
    if(!length || length>1023) return false;
    char *text=malloc(length+1);if(!text) return false;
    bool ok=false;unsigned seen=0;
    if(httpd_req_get_url_query_str(req,text,length+1)!=ESP_OK) goto end;
    char *p=text;
    while(p && *p) {
        char *next=strchr(p,'&');if(next) *next++=0;
        char *value=strchr(p,'=');if(!value) goto end;*value++=0;
        char key[16],decoded[256];if(!decode(p,key,sizeof(key)) || !decode(value,decoded,sizeof(decoded))) goto end;
        unsigned bit;
        if(!strcmp(key,"album")) {bit=1;if(!decoded[0] || strlen(decoded)>=sizeof(out->album)) goto end;strcpy(out->album,decoded);}
        else if(photo && !strcmp(key,"name")) {bit=2;char output[256];if(!epaper_photo_name_valid(decoded) && (strncmp(req->uri,"/api/upload",11) || !epaper_image_upload_name(decoded,output,sizeof(output)))) goto end;strcpy(out->name,decoded);}
        else if(!photo && !strcmp(key,"offset")) {bit=4;if(!integer(decoded,EPAPER_MAX_PHOTOS,&out->offset)) goto end;}
        else if(!photo && !strcmp(key,"limit")) {bit=8;if(!integer(decoded,100,&out->limit) || !out->limit) goto end;}
        else goto end;
        if(seen&bit) goto end;
        seen|=bit;p=next;
    }
    ok=(seen&1) && (!photo || (seen&2));
end:
    free(text);return ok;
}
static esp_err_t denied(httpd_req_t *req)
{return error_response(req,"403 Forbidden","token_required","Use X-Album-Token from GET /api/status");}
static esp_err_t busy(httpd_req_t *req)
{return error_response(req,"409 Conflict","busy","Display, configuration or SD transfer is busy; retry after completion");}
static esp_err_t albums(httpd_req_t *req)
{
    if(!epaper_http_authorized(req)) return denied(req);
    if(httpd_req_get_url_query_len(req)) return error_response(req,"400 Bad Request","bad_query","No query parameters expected");
    if(!epaper_http_file_begin()) return busy(req);
    const epaper_album_config_t *config=epaper_album_configuration();
    esp_err_t err;
    if(!config) err=error_response(req,"503 Service Unavailable","album_unavailable","Album configuration is unavailable");
    else {
        cJSON *doc=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(doc,"albums");
        cJSON_AddStringToObject(doc,"root",config->root);
        for(unsigned i=0;i<config->album_count;i++) {
            const epaper_album_definition_t *a=&config->albums[i];cJSON *row=cJSON_CreateObject();
            if(!row || !cJSON_AddItemToArray(list,row)) {cJSON_Delete(row);cJSON_Delete(doc);doc=NULL;break;}
            cJSON_AddStringToObject(row,"id",a->id);cJSON_AddStringToObject(row,"directory",a->directory);
            cJSON_AddBoolToObject(row,"enabled",a->enabled);cJSON_AddNumberToObject(row,"interval_seconds",a->interval);
        }
        err=json_response(req,"200 OK",doc);
    }
    epaper_http_file_end(false);return err;
}
static esp_err_t files(httpd_req_t *req)
{
    if(!epaper_http_authorized(req)) return denied(req);
    query_t q;if(!query(req,false,&q)) return error_response(req,"400 Bad Request","bad_query","Expected album, optional offset and limit=1..100");
    if(!epaper_http_file_begin()) return busy(req);
    const epaper_album_config_t *config=epaper_album_configuration();
    epaper_file_entry_t *entries=NULL;size_t count=0;esp_err_t err;
    epaper_file_result_t result=epaper_files_list(EPAPER_SD_MOUNT,config,q.album,&entries,&count);
    if(result!=FILE_LIST_OK) {
        const char *status=result==FILE_LIST_INVALID||result==FILE_LIST_NOT_FOUND?"404 Not Found":result==FILE_LIST_LIMIT?"413 Content Too Large":"500 Internal Server Error";
        err=error_response(req,status,"list_failed","Unknown album, missing directory, too many files or SD read failure");
    } else {
        cJSON *doc=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(doc,"files");
        cJSON_AddStringToObject(doc,"album",q.album);cJSON_AddNumberToObject(doc,"total",(double)count);
        cJSON_AddNumberToObject(doc,"offset",q.offset);cJSON_AddNumberToObject(doc,"limit",q.limit);
        size_t end=(size_t)q.offset+q.limit;if(end>count) end=count;
        if(end<count) cJSON_AddNumberToObject(doc,"next_offset",(double)end);else cJSON_AddNullToObject(doc,"next_offset");
        for(size_t i=q.offset;i<end;i++) {
            cJSON *row=cJSON_CreateObject();
            if(!row || !cJSON_AddItemToArray(list,row)) {cJSON_Delete(row);cJSON_Delete(doc);doc=NULL;break;}
            cJSON_AddStringToObject(row,"name",entries[i].name);cJSON_AddNumberToObject(row,"bytes",(double)entries[i].bytes);
            cJSON_AddBoolToObject(row,"header_valid",entries[i].header_valid);
        }
        err=json_response(req,"200 OK",doc);
    }
    free(entries);epaper_http_file_end(false);return err;
}
static esp_err_t upload(httpd_req_t *req,const query_t *q,const char *path,
    const epaper_album_config_t *config,bool *changed)
{
    char type[64];
    if(httpd_req_get_hdr_value_str(req,"Content-Type",type,sizeof(type))!=ESP_OK ||
       (strcmp(type,"image/bmp") && strcmp(type,"application/octet-stream")))
        return error_response(req,"415 Unsupported Media Type","binary_required","Send raw BMP bytes, not multipart/form-data");
    if(req->content_len>MAX_UPLOAD) return error_response(req,"413 Content Too Large","upload_limit","Maximum upload size is 2 MiB");
    if(req->content_len<54) return error_response(req,"422 Unprocessable Content","invalid_bmp","BMP is too small");
    struct stat st;if(!stat(path,&st)) return error_response(req,"409 Conflict","file_exists","Existing files are not overwritten");
    epaper_album_status_t state;epaper_album_status(&state);
    for(unsigned i=0;i<config->album_count;i++) if(!strcmp(q->album,config->albums[i].id) &&
       config->albums[i].enabled && state.photos>=EPAPER_MAX_PHOTOS)
        return error_response(req,"409 Conflict","catalog_limit","Enabled albums already contain the maximum indexed photos");
    epaper_file_entry_t *entries=NULL;size_t count=0;
    epaper_file_result_t listed=epaper_files_list(EPAPER_SD_MOUNT,config,q->album,&entries,&count);free(entries);
    if(listed!=FILE_LIST_OK || count>=EPAPER_MAX_PHOTOS)
        return error_response(req,"409 Conflict","album_limit","Cannot index album directory or album is full");
    uint64_t total=0,free_bytes=0;
    if(esp_vfs_fat_info(EPAPER_SD_MOUNT,&total,&free_bytes)!=ESP_OK || free_bytes<(uint64_t)req->content_len+65536)
        return error_response(req,"507 Insufficient Storage","sd_full","SD card has insufficient free space");
    char directory[EPAPER_PATH_MAX],temporary[EPAPER_PATH_MAX+32];
    if(!epaper_album_file_path(EPAPER_SD_MOUNT,config,q->album,NULL,directory,sizeof(directory)))
        return error_response(req,"404 Not Found","album_missing","Unknown album");
    int n=snprintf(temporary,sizeof(temporary),"%s/.epaper-upload.tmp",directory);
    if(n<0 || (size_t)n>=sizeof(temporary)) return error_response(req,"400 Bad Request","path_too_long","Path is too long");
    uint8_t *buffer=malloc(CHUNK_BYTES);if(!buffer) return error_response(req,"503 Service Unavailable","memory","Not enough transfer memory");
    FILE *f=fopen(temporary,"wb");
    if(!f) {free(buffer);return error_response(req,"507 Insufficient Storage","sd_write_failed","Cannot create upload temporary file");}
    /* Batch FAT reads/writes rather than many small newlib VFS operations. */
    if(setvbuf(f,NULL,_IOFBF,FILE_BUFFER_BYTES))
        ESP_LOGW("http-files","Upload uses default file buffer");
    const char *status="400 Bad Request",*code="upload_incomplete",*message="Upload interrupted; partial data was discarded";
    size_t received=0;bool ok=true;int64_t deadline=esp_timer_get_time()+TRANSFER_US;
    while(received<req->content_len) {
        if(esp_timer_get_time()>deadline) {ok=false;status="408 Request Timeout";code="transfer_timeout";break;}
        size_t length=req->content_len-received;if(length>CHUNK_BYTES) length=CHUNK_BYTES;
        int got=httpd_req_recv(req,(char*)buffer,length);
        if(got<=0) {ok=false;if(got==HTTPD_SOCK_ERR_TIMEOUT) status="408 Request Timeout";break;}
        if(fwrite(buffer,1,(size_t)got,f)!=(size_t)got) {ok=false;status="507 Insufficient Storage";code="sd_write_failed";break;}
        received+=(size_t)got;
    }
    if(fflush(f) || fsync(fileno(f))) {ok=false;status="507 Insufficient Storage";code="sd_write_failed";}
    if(fclose(f)) {ok=false;status="507 Insufficient Storage";code="sd_write_failed";}
    free(buffer);
    if(ok && !epaper_photo_validate(temporary,config->rotation)) {
        ok=false;status="422 Unprocessable Content";code="invalid_bmp";
        message="Require prepared 24-bit uncompressed BMP with correct dimensions and exactly the supported six-color palette";
    }
    if(ok && rename(temporary,path)) {ok=false;status="507 Insufficient Storage";code="sd_install_failed";message="Cannot install validated photo";}
    if(!ok) {remove(temporary);return error_response(req,status,code,message);}
    *changed=true;cJSON *doc=cJSON_CreateObject();
    cJSON_AddStringToObject(doc,"album",q->album);cJSON_AddStringToObject(doc,"name",q->name);
    cJSON_AddNumberToObject(doc,"bytes",(double)received);cJSON_AddBoolToObject(doc,"index_pending",true);
    return json_response(req,"201 Created",doc);
}
static esp_err_t download(httpd_req_t *req,const char *path)
{
    FILE *f=fopen(path,"rb");if(!f) return error_response(req,"404 Not Found","photo_missing","Photo does not exist");
    if(setvbuf(f,NULL,_IOFBF,FILE_BUFFER_BYTES))
        ESP_LOGW("http-files","Download uses default file buffer");
    uint8_t *buffer=malloc(CHUNK_BYTES);
    if(!buffer) {fclose(f);return error_response(req,"503 Service Unavailable","memory","Not enough transfer memory");}
    httpd_resp_set_type(req,"image/bmp");httpd_resp_set_hdr(req,"Content-Disposition","attachment");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    esp_err_t err=ESP_OK;size_t count,sent=0;int64_t start=esp_timer_get_time(),deadline=start+TRANSFER_US;
    while((count=fread(buffer,1,CHUNK_BYTES,f))) {
        if(esp_timer_get_time()>deadline) {err=ESP_ERR_TIMEOUT;break;}
        err=httpd_resp_send_chunk(req,(char*)buffer,count);if(err!=ESP_OK) break;sent+=count;
    }
    if(ferror(f)) err=ESP_FAIL;
    fclose(f);free(buffer);
    if(err==ESP_OK) err=httpd_resp_send_chunk(req,NULL,0);
    ESP_LOGI("http-files","Download bytes=%u elapsed_ms=%lld result=%s",(unsigned)sent,(long long)((esp_timer_get_time()-start)/1000),esp_err_to_name(err));
    return err;
}
static esp_err_t photo(httpd_req_t *req)
{
    if(!epaper_http_authorized(req)) return denied(req);
    /* Chunk headers must not wait for delayed ACKs before the pixel payload. */
    int no_delay=1;
    if(setsockopt(httpd_req_to_sockfd(req),IPPROTO_TCP,TCP_NODELAY,&no_delay,sizeof(no_delay))<0)
        return error_response(req,"500 Internal Server Error","socket_config","Cannot configure transfer socket");
    query_t q;if(!query(req,true,&q)) return error_response(req,"400 Bad Request","bad_query","Expected album and a safe BMP filename");
    if(req->method==HTTP_PUT && epaper_image_active())return error_response(req,"409 Conflict","busy","Photo conversion is active");
    if(req->method!=HTTP_PUT && req->content_len) return error_response(req,"400 Bad Request","body_not_allowed","GET/DELETE must have no request body");
    if(!epaper_http_file_begin()) return busy(req);
    const epaper_album_config_t *config=epaper_album_configuration();char path[EPAPER_PATH_MAX];
    bool changed=false;esp_err_t err;struct stat st;
    if(!epaper_album_file_path(EPAPER_SD_MOUNT,config,q.album,q.name,path,sizeof(path)))
        err=error_response(req,"404 Not Found","album_missing","Unknown album or path is too long");
    else if(req->method==HTTP_PUT) err=upload(req,&q,path,config,&changed);
    else if(stat(path,&st) || (st.st_mode&S_IFMT)!=S_IFREG)
        err=error_response(req,"404 Not Found","photo_missing","Photo does not exist");
    else if(req->method==HTTP_GET) err=download(req,path);
    else if(remove(path)) err=error_response(req,"500 Internal Server Error","sd_delete_failed","Cannot delete SD photo");
    else {
        changed=true;cJSON *doc=cJSON_CreateObject();cJSON_AddBoolToObject(doc,"deleted",true);
        cJSON_AddStringToObject(doc,"album",q.album);cJSON_AddStringToObject(doc,"name",q.name);
        cJSON_AddBoolToObject(doc,"index_pending",true);err=json_response(req,"200 OK",doc);
    }
    epaper_http_file_end(changed);return err;
}
static esp_err_t import_photo(httpd_req_t *req)
{
    if(!epaper_http_authorized(req))return denied(req);
    if(req->method==HTTP_GET){cJSON *doc=cJSON_CreateObject();if(doc){epaper_image_json(doc);cJSON_AddBoolToObject(doc,"can_upload",!epaper_image_active()&&epaper_http_file_available(120));}return json_response(req,"200 OK",doc);}
    if(req->content_len<54 || req->content_len>8u*1024u*1024u)return error_response(req,"413 Content Too Large","upload_limit","Source photo must be 54 bytes to 8 MiB");
    query_t q;char name[256],target[EPAPER_PATH_MAX],source[EPAPER_PATH_MAX],error[160]={0};
    if(!query(req,true,&q) || !epaper_image_upload_name(q.name,name,sizeof(name)))return error_response(req,"400 Bad Request","bad_query","Use a safe JPEG, PNG or BMP filename");
    if(epaper_image_active() || !epaper_http_file_available(120) || !epaper_http_file_begin_for(120))return busy(req);
    const epaper_album_config_t *config=epaper_album_configuration();epaper_file_entry_t *entries=NULL;size_t count=0;uint64_t total=0,free_bytes=0;esp_err_t err=ESP_OK;FILE *f=NULL;uint8_t *buffer=NULL;bool reserved=false,accepted=false,sd_held=true;
    unsigned job=0;
    if(!epaper_album_file_path(EPAPER_SD_MOUNT,config,q.album,name,target,sizeof(target))){err=error_response(req,"404 Not Found","album_missing","Unknown album");goto end;}
    if(epaper_files_list(EPAPER_SD_MOUNT,config,q.album,&entries,&count)!=FILE_LIST_OK || count>=EPAPER_MAX_PHOTOS){err=error_response(req,"409 Conflict","album_limit","Album is full or inaccessible");goto end;}
    epaper_album_status_t current;epaper_album_status(&current);
    for(unsigned i=0;i<config->album_count;i++)if(!strcmp(config->albums[i].id,q.album) && config->albums[i].enabled && current.photos>=EPAPER_MAX_PHOTOS){err=error_response(req,"409 Conflict","album_limit","Playback photo limit reached");goto end;}
    if(esp_vfs_fat_info(EPAPER_SD_MOUNT,&total,&free_bytes)!=ESP_OK || free_bytes<(uint64_t)req->content_len+1152054+65536){err=error_response(req,"507 Insufficient Storage","sd_full","Insufficient space for source and converted BMP");goto end;}
    esp_err_t reserve=epaper_image_receive(q.album,q.name,target,config->rotation,source,sizeof(source),&job,error,sizeof(error));
    if(reserve!=ESP_OK){
        if(reserve==ESP_ERR_INVALID_STATE)err=epaper_image_active()?busy(req):error_response(req,"409 Conflict","exists","Output BMP already exists");
        else err=error_response(req,"500 Internal Server Error","sd_access_failed","Cannot access output BMP path on SD");
        goto end;
    }reserved=true;
    f=fopen(source,"wb");buffer=malloc(CHUNK_BYTES);if(!f || !buffer){err=error_response(req,"507 Insufficient Storage","sd_write_failed","Cannot stage source photo");goto end;}
    (void)setvbuf(f,NULL,_IOFBF,FILE_BUFFER_BYTES);
    epaper_http_sd_unlock();sd_held=false;
    size_t received=0;int64_t deadline=esp_timer_get_time()+100000000;
    while(received<req->content_len) {
        size_t chunk=req->content_len-received;if(chunk>CHUNK_BYTES)chunk=CHUNK_BYTES;
        int got=httpd_req_recv(req,(char*)buffer,chunk);
        if(got<=0 || esp_timer_get_time()>deadline){err=error_response(req,"408 Request Timeout","upload_incomplete","Source upload interrupted; partial data discarded");goto end;}
        while(!epaper_http_sd_try_lock())vTaskDelay(pdMS_TO_TICKS(5));
        bool wrote=fwrite(buffer,1,(size_t)got,f)==(size_t)got;epaper_http_sd_unlock();
        if(!wrote){err=error_response(req,"507 Insufficient Storage","sd_write_failed","Cannot write source photo");goto end;}
        received+=(size_t)got;epaper_image_received(received);vTaskDelay(1);
    }
    while(!epaper_http_sd_try_lock())vTaskDelay(pdMS_TO_TICKS(5));
    sd_held=true;
    bool flushed=!fflush(f) && !fsync(fileno(f));if(fclose(f))flushed=false;f=NULL;
    if(!flushed){err=error_response(req,"507 Insufficient Storage","sd_write_failed","Cannot flush source photo");goto end;}
    if(epaper_image_submit()!=ESP_OK){err=error_response(req,"503 Service Unavailable","memory","Cannot start conversion");goto end;}
    accepted=true;cJSON *doc=cJSON_CreateObject();cJSON_AddNumberToObject(doc,"job_id",job);cJSON_AddStringToObject(doc,"name",name);cJSON_AddStringToObject(doc,"state","queued");err=json_response(req,"202 Accepted",doc);
end:
    if(!sd_held)while(!epaper_http_sd_try_lock())vTaskDelay(pdMS_TO_TICKS(5));
    if(f)fclose(f);
    free(buffer);free(entries);
    if(reserved && !accepted){remove(source);epaper_image_abort("Source upload failed; no BMP was published");}
    epaper_http_file_end(false);return err;
}
esp_err_t epaper_http_register_files(httpd_handle_t server)
{
    const httpd_uri_t routes[]={
        {.uri="/api/upload",.method=HTTP_PUT,.handler=import_photo},
        {.uri="/api/upload",.method=HTTP_GET,.handler=import_photo},
        {.uri="/api/albums",.method=HTTP_GET,.handler=albums},
        {.uri="/api/files",.method=HTTP_GET,.handler=files},
        {.uri="/api/photo",.method=HTTP_GET,.handler=photo},
        {.uri="/api/photo",.method=HTTP_PUT,.handler=photo},
        {.uri="/api/photo",.method=HTTP_DELETE,.handler=photo},
    };
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);i++) {
        esp_err_t err=httpd_register_uri_handler(server,&routes[i]);if(err!=ESP_OK) return err;
    }
    return ESP_OK;
}

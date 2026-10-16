#include "epaper_image_import.h"
#include "epaper_image.h"
#include "epaper_http.h"
#include "epaper_runtime.h"
#include "epaper_http_internal.h"
#include "epaper_announcement.h"
#include "epaper_network.h"
#include "epaper_storage.h"
#include "epaper_profile.h"
#include "tjpgd.h"
#include "png_import.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static portMUX_TYPE state_lock=portMUX_INITIALIZER_UNLOCKED;
static struct {
    unsigned id,progress,width,height,source_width,source_height;
    bool active,published;
    char stage[24],album[EPAPER_ID_MAX],source_name[256],output_name[256],error[160];
    size_t received,workspace;
    int64_t started,processing_started,ended,paused,decode_ms,dither_ms,write_ms,validate_ms;
} state;
static char source_path[EPAPER_PATH_MAX],output_path[EPAPER_PATH_MAX],temporary_path[EPAPER_PATH_MAX];
static int target_rotation;
static TaskHandle_t task;
static void phase(const char *stage,unsigned progress)
{portENTER_CRITICAL(&state_lock);snprintf(state.stage,sizeof(state.stage),"%s",stage);state.progress=progress;portEXIT_CRITICAL(&state_lock);}
bool epaper_image_active(void)
{portENTER_CRITICAL(&state_lock);bool active=state.active;portEXIT_CRITICAL(&state_lock);return active;}
static void dimensions(unsigned sw,unsigned sh,unsigned w,unsigned h,size_t workspace)
{portENTER_CRITICAL(&state_lock);state.source_width=sw;state.source_height=sh;state.width=w;state.height=h;state.workspace=workspace;portEXIT_CRITICAL(&state_lock);}
static void finish(const char *message)
{
    portENTER_CRITICAL(&state_lock);state.active=false;state.ended=esp_timer_get_time();snprintf(state.stage,sizeof(state.stage),"%s",message?"failed":"succeeded");if(!message)state.progress=100;snprintf(state.error,sizeof(state.error),"%s",message?message:"");task=NULL;portEXIT_CRITICAL(&state_lock);
}
void epaper_image_abort(const char *message){finish(message);}
void epaper_image_received(size_t bytes){portENTER_CRITICAL(&state_lock);state.received=bytes;portEXIT_CRITICAL(&state_lock);}
bool epaper_image_upload_name(const char *name,char *output,size_t size)
{
    if(!name)return false;
    const char *dot=strrchr(name,'.');if(!dot)return false;
    char ext[8];size_t count=strlen(dot);if(count>=sizeof(ext))return false;
    for(size_t i=0;i<=count;i++)ext[i]=(char)tolower((unsigned char)dot[i]);
    if(strcmp(ext,".jpg") && strcmp(ext,".jpeg") && strcmp(ext,".png") && strcmp(ext,".bmp"))return false;
    size_t base=(size_t)(dot-name);if(!base || base+5>size)return false;
    memcpy(output,name,base);memcpy(output+base,".bmp",5);return epaper_photo_name_valid(output);
}
esp_err_t epaper_image_receive(const char *album,const char *name,const char *path,int rotation,char *source,size_t size,unsigned *job,char *error,size_t error_size)
{
    char output_name[256];if(!epaper_image_upload_name(name,output_name,sizeof(output_name)))return ESP_ERR_INVALID_ARG;
    if(epaper_image_active())return ESP_ERR_INVALID_STATE;
    struct stat st;if(!stat(path,&st) || errno!=ENOENT){snprintf(error,error_size,"Output BMP already exists or is inaccessible");return ESP_ERR_INVALID_STATE;}
    const char *slash=strrchr(path,'/');if(!slash)return ESP_ERR_INVALID_ARG;size_t prefix=(size_t)(slash-path);
    int a=snprintf(source_path,sizeof(source_path),"%.*s/.epaper-import-source.tmp",(int)prefix,path);
    int b=snprintf(temporary_path,sizeof(temporary_path),"%.*s/.epaper-import-output.tmp",(int)prefix,path);
    if(a<=0 || b<=0 || (size_t)a>=sizeof(source_path) || (size_t)b>=sizeof(temporary_path) || strlen(source_path)>=size)return ESP_ERR_INVALID_ARG;
    /* Never resume or publish an interrupted source or partially converted BMP. */
    remove(source_path);remove(temporary_path);snprintf(output_path,sizeof(output_path),"%s",path);target_rotation=rotation;strcpy(source,source_path);
    portENTER_CRITICAL(&state_lock);
    unsigned id=state.id+1;memset(&state,0,sizeof(state));state.id=id;state.active=true;state.started=esp_timer_get_time();strcpy(state.stage,"receiving");snprintf(state.album,sizeof(state.album),"%s",album);snprintf(state.source_name,sizeof(state.source_name),"%s",name);strcpy(state.output_name,output_name);*job=id;
    portEXIT_CRITICAL(&state_lock);return ESP_OK;
}
/* All SD operations are short and yield to the announcement/display task. */
static bool gate(void)
{
    int64_t blocked=0;char previous[24]={0};
    for(;;) {
        if(!epaper_network_provisioning() && epaper_announcement_admit(3) && epaper_http_sd_try_lock()) {
            if(blocked){portENTER_CRITICAL(&state_lock);state.paused+=esp_timer_get_time()-blocked;snprintf(state.stage,sizeof(state.stage),"%s",previous);portEXIT_CRITICAL(&state_lock);}
            return true;
        }
        if(!blocked){blocked=esp_timer_get_time();portENTER_CRITICAL(&state_lock);snprintf(previous,sizeof(previous),"%s",state.stage);strcpy(state.stage,"waiting");portEXIT_CRITICAL(&state_lock);}
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static bool read_at(FILE *f,uint64_t offset,void *buffer,size_t size)
{
    gate();bool ok=offset<=INT32_MAX && !fseek(f,(long)offset,SEEK_SET) && fread(buffer,1,size,f)==size;epaper_http_sd_unlock();return ok;
}
static unsigned orientation(FILE *f)
{
    unsigned result=1;uint64_t offset=2;uint8_t header[4];
    for(unsigned segment=0;segment<128;segment++) {
        if(!read_at(f,offset,header,4) || header[0]!=0xff || header[1]==0xda || header[1]==0xd9)break;
        unsigned size=(header[2]<<8)|header[3];if(size<2)break;
        if(header[1]==0xe1){uint8_t *data=heap_caps_malloc(size-2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(data){if(read_at(f,offset+4,data,size-2))result=epaper_image_exif_orientation(data,size-2);free(data);}break;}
        offset+=2+size;
    }
    return result;
}
typedef struct {FILE *file;epaper_image_map_t *map;uint16_t *accum;unsigned blocks;} jpeg_io_t;
static size_t jpeg_read(JDEC *decoder,uint8_t *buffer,size_t count)
{
    jpeg_io_t *io=decoder->device;gate();size_t done;
    if(buffer)done=(size_t)fread(buffer,1,count,io->file);else done=fseek(io->file,(long)count,SEEK_CUR)?0:count;
    epaper_http_sd_unlock();return done;
}
static int jpeg_output(JDEC *decoder,void *bitmap,JRECT *rect)
{
    jpeg_io_t *io=decoder->device;const epaper_image_map_t *m=io->map;
    gate();epaper_http_sd_unlock();
    epaper_image_add_block(m,io->accum,bitmap,rect->left,rect->right,rect->top,rect->bottom);
    if(!(++io->blocks%256)){phase("decoding",5+25u*(rect->bottom+1)/m->source_height);vTaskDelay(1);}return 1;
}
static void png_lock(void){gate();}
static void png_progress(unsigned sw,unsigned sh,unsigned progress,size_t workspace)
{dimensions(sw,sh,(target_rotation==90||target_rotation==270)?480:800,(target_rotation==90||target_rotation==270)?800:480,workspace);phase("decoding",progress);}
static void worker(void *unused)
{
    (void)unused;epaper_profile_phase(PROFILE_IMPORT,true);FILE *input=NULL,*output=NULL;uint8_t *rgb=NULL,*row=NULL,*source_rows=NULL;int32_t *errors=NULL;void *pool=NULL;epaper_image_map_t *map=NULL;
    char png_error[160]={0};const char *failure="Image conversion failed";bool published=false;int64_t processing_start=esp_timer_get_time();
    portENTER_CRITICAL(&state_lock);state.processing_started=processing_start;portEXIT_CRITICAL(&state_lock);
    unsigned w=(target_rotation==90||target_rotation==270)?480:800,h=(target_rotation==90||target_rotation==270)?800:480;
    size_t pixels=(size_t)w*h;map=heap_caps_malloc(sizeof(*map),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!map){failure="Insufficient PSRAM for image map";goto end;}
    phase("decoding",3);gate();input=fopen(source_path,"rb");struct stat st;bool have_size=stat(source_path,&st)==0;epaper_http_sd_unlock();
    if(!input || !have_size){failure="Source photo cannot be read";goto end;}
    uint8_t header[54];if(!read_at(input,0,header,sizeof(header))){failure="Source photo is truncated";goto end;}
    if(header[0]==0xff && header[1]==0xd8) {
        pool=heap_caps_malloc(4096,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);if(!pool){failure="Insufficient internal JPEG workspace";goto end;}
        JDEC decoder;jpeg_io_t io={.file=input,.map=map};read_at(input,0,header,0);
        JRESULT result=jd_prepare(&decoder,jpeg_read,pool,4096,&io);
        if(result!=JDR_OK){failure="JPEG must be supported baseline JPEG; progressive/CMYK/damaged files are not supported";goto end;}
        if(decoder.width>EPAPER_IMAGE_MAX_EDGE || decoder.height>EPAPER_IMAGE_MAX_EDGE || (uint64_t)decoder.width*decoder.height>EPAPER_IMAGE_MAX_PIXELS){failure="JPEG exceeds 12000 pixels per edge or 40 megapixels";goto end;}
        unsigned orient=orientation(input);read_at(input,0,header,0);
        /* Metadata scanning changes FILE position: prepare the stream again. */
        result=jd_prepare(&decoder,jpeg_read,pool,4096,&io);if(result!=JDR_OK)goto end;
        unsigned scale=0,ow=orient>=5?decoder.height:decoder.width,oh=orient>=5?decoder.width:decoder.height;
        while(scale<3 && (ow>>(scale+1))>=w && (oh>>(scale+1))>=h)scale++;
        if(!epaper_image_map(map,decoder.width>>scale,decoder.height>>scale,w,h,orient))goto end;
        rgb=heap_caps_calloc(pixels*3,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!rgb){failure="Insufficient PSRAM for resized JPEG";goto end;}
        io.accum=(uint16_t*)rgb;dimensions(decoder.width,decoder.height,w,h,pixels*6+sizeof(*map)+4096+((size_t)w+2)*24+w*3+32768);
        result=jd_decomp(&decoder,jpeg_output,(uint8_t)scale);if(result!=JDR_OK){failure="JPEG decoding failed or input was interrupted";goto end;}
        for(size_t i=0;i<pixels*3;i++)rgb[i]=(uint8_t)(((uint16_t*)rgb)[i]>>8);
        free(pool);pool=NULL;
    } else if(!memcmp(header,"\x89PNG\r\n\x1a\n",8)) {
        if(!read_at(input,0,header,0)){failure="PNG source cannot be rewound";goto end;}
        const epaper_png_hooks_t hooks={.lock=png_lock,.unlock=epaper_http_sd_unlock,.report=png_progress};
        if(!epaper_png_decode(input,map,w,h,&rgb,&hooks,png_error,sizeof(png_error))){failure=png_error;goto end;}
    } else {
        epaper_image_bmp_t bmp;if(!epaper_image_bmp_header(header,(uint64_t)st.st_size,&bmp)){failure="Require 24-bit uncompressed BMP with valid dimensions and complete pixels";goto end;}
        if(!epaper_image_map(map,bmp.width,bmp.height,w,h,1))goto end;
        rgb=heap_caps_malloc(pixels*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);source_rows=heap_caps_malloc((size_t)bmp.stride*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!rgb || !source_rows){failure="Insufficient PSRAM for resized BMP";goto end;}
        dimensions(bmp.width,bmp.height,w,h,pixels*3+(size_t)bmp.stride*2+sizeof(*map)+((size_t)w+2)*24+w*3+32768);
        phase("resizing",10);
        for(unsigned y=0;y<h;y++) {
            unsigned ys[2]={map->y[y].low,map->y[y].high};
            for(unsigned k=0;k<2;k++){unsigned file_y=bmp.top_down?ys[k]:bmp.height-1-ys[k];if(!read_at(input,(uint64_t)bmp.offset+(uint64_t)file_y*bmp.stride,source_rows+(size_t)k*bmp.stride,bmp.stride)){failure="BMP row is truncated or SD read failed";goto end;}}
            for(unsigned x=0;x<w;x++) {
                unsigned weights[4];epaper_image_weights(map,x,y,weights);
                for(unsigned c=0;c<3;c++){unsigned value=0;for(unsigned k=0;k<4;k++)value+=source_rows[(size_t)(k>>1)*bmp.stride+((k&1)?map->x[x].high:map->x[x].low)*3+2-c]*weights[k];rgb[((size_t)y*w+x)*3+c]=(uint8_t)(value>>8);}
            }
            phase("resizing",10+20u*(y+1)/h);if(!(y%8))vTaskDelay(1);
        }
        free(source_rows);source_rows=NULL;
    }
    portENTER_CRITICAL(&state_lock);state.decode_ms=(esp_timer_get_time()-processing_start)/1000;portEXIT_CRITICAL(&state_lock);
    gate();fclose(input);input=NULL;epaper_http_sd_unlock();
    errors=heap_caps_calloc(((size_t)w+2)*6,sizeof(*errors),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);row=heap_caps_malloc(w*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!errors || !row){failure="Insufficient PSRAM for six-color dithering";goto end;}
    int64_t stage_start=esp_timer_get_time();phase("dithering",30);uint8_t lut[256];epaper_image_tone(rgb,pixels,lut);

    int32_t *cur=errors,*next=errors+(w+2)*3;
    for(unsigned y=0;y<h;y++) {
        gate();epaper_http_sd_unlock();epaper_image_dither_row(rgb+(size_t)y*w*3,w,y,lut,cur,next,row);
        memcpy(rgb+(size_t)y*w*3,row,w*3);
        int32_t *swap=cur;cur=next;next=swap;phase("dithering",30+50u*(y+1)/h);if(!(y%8))vTaskDelay(1);
    }
    portENTER_CRITICAL(&state_lock);state.dither_ms=(esp_timer_get_time()-stage_start)/1000;portEXIT_CRITICAL(&state_lock);
    free(errors);errors=NULL;free(row);row=NULL;
    stage_start=esp_timer_get_time();phase("writing",80);
    gate();output=fopen(temporary_path,"wb");
    if(output){(void)setvbuf(output,NULL,_IOFBF,32768);epaper_image_bmp_output_header(header,w,h);if(fwrite(header,1,54,output)!=54){fclose(output);output=NULL;}}
    epaper_http_sd_unlock();if(!output){failure="Cannot create converted BMP on SD";goto end;}
    for(unsigned y=0;y<h;y++) {
        gate();bool ok=fwrite(rgb+(size_t)(h-1-y)*w*3,1,w*3,output)==w*3;epaper_http_sd_unlock();
        if(!ok){failure="SD write failed during conversion";goto end;}
        phase("writing",80+10u*(y+1)/h);if(!(y%16))vTaskDelay(1);
    }
    gate();bool flushed=!fflush(output) && !fsync(fileno(output));if(fclose(output))flushed=false;output=NULL;epaper_http_sd_unlock();
    if(!flushed){failure="Converted BMP could not be flushed to SD";goto end;}
    portENTER_CRITICAL(&state_lock);state.write_ms=(esp_timer_get_time()-stage_start)/1000;portEXIT_CRITICAL(&state_lock);
    free(rgb);rgb=NULL;free(errors);errors=NULL;free(row);row=NULL;free(map);map=NULL;
    stage_start=esp_timer_get_time();phase("validating",92);gate();bool valid=epaper_photo_validate(temporary_path,target_rotation);epaper_http_sd_unlock();if(!valid){failure="Converted BMP failed format/palette validation";goto end;}
    portENTER_CRITICAL(&state_lock);state.validate_ms=(esp_timer_get_time()-stage_start)/1000;portEXIT_CRITICAL(&state_lock);
    phase("publishing",97);
    while(!epaper_http_file_begin_for(3))vTaskDelay(pdMS_TO_TICKS(20));
    unsigned revision=epaper_http_file_revision()+1;
    bool absent=stat(output_path,&st)!=0 && errno==ENOENT;
    published=absent && rename(temporary_path,output_path)==0;
    epaper_http_file_end(published);
    if(!published){failure="Output BMP already exists or SD install failed";goto end;}
    portENTER_CRITICAL(&state_lock);state.published=true;portEXIT_CRITICAL(&state_lock);
    phase("indexing",99);char index_error[160]={0};while(!epaper_http_index_ready(revision,index_error,sizeof(index_error)))vTaskDelay(pdMS_TO_TICKS(50));
    if(index_error[0]){failure="BMP saved; playback index failed, rescan is required";goto end;}
    failure=NULL;
end:
    gate();if(input)fclose(input);if(output)fclose(output);remove(source_path);if(!published)remove(temporary_path);epaper_http_sd_unlock();
    free(rgb);free(row);free(source_rows);free(errors);free(pool);free(map);
    ESP_LOGI("image-import","Job complete: wall_ms=%lld result=%s",(long long)((esp_timer_get_time()-processing_start)/1000),failure?failure:"ok");
    epaper_profile_phase(PROFILE_IMPORT,false);finish(failure);vTaskDelete(NULL);
}
esp_err_t epaper_image_submit(void)
{
    phase("queued",1);
    if(xTaskCreate(worker,"photo_import",12288,NULL,1,&task)!=pdPASS){epaper_image_abort("Cannot start image conversion task");return ESP_ERR_NO_MEM;}
    return ESP_OK;
}
void epaper_image_json(cJSON *root)
{
    typeof(state) copy;portENTER_CRITICAL(&state_lock);copy=state;portEXIT_CRITICAL(&state_lock);
    cJSON_AddNumberToObject(root,"job_id",copy.id);cJSON_AddBoolToObject(root,"active",copy.active);cJSON_AddBoolToObject(root,"published",copy.published);cJSON_AddStringToObject(root,"state",copy.id?copy.stage:"idle");cJSON_AddNumberToObject(root,"progress_percent",copy.progress);cJSON_AddStringToObject(root,"album",copy.album);cJSON_AddStringToObject(root,"source_name",copy.source_name);cJSON_AddStringToObject(root,"name",copy.output_name);cJSON_AddStringToObject(root,"error",copy.error);
    cJSON_AddNumberToObject(root,"decode_resize_ms",copy.decode_ms);cJSON_AddNumberToObject(root,"dither_ms",copy.dither_ms);cJSON_AddNumberToObject(root,"write_ms",copy.write_ms);cJSON_AddNumberToObject(root,"validate_ms",copy.validate_ms);
    cJSON_AddNumberToObject(root,"elapsed_ms",copy.started?((copy.ended?copy.ended:esp_timer_get_time())-copy.started)/1000:0);cJSON_AddNumberToObject(root,"paused_ms",copy.paused/1000);cJSON_AddNumberToObject(root,"conversion_ms",copy.processing_started?((copy.ended?copy.ended:esp_timer_get_time())-copy.processing_started)/1000:0);cJSON_AddNumberToObject(root,"source_width",copy.source_width);cJSON_AddNumberToObject(root,"source_height",copy.source_height);cJSON_AddNumberToObject(root,"width",copy.width);cJSON_AddNumberToObject(root,"height",copy.height);cJSON_AddNumberToObject(root,"workspace_bytes",copy.workspace);
}

void epaper_image_cleanup(void)
{
    const epaper_album_config_t *c=epaper_album_configuration();if(!c)return;
    for(unsigned i=0;i<c->album_count;i++)for(unsigned k=0;k<2;k++) {
        char folder[EPAPER_PATH_MAX],path[EPAPER_PATH_MAX+40];struct stat st;
        if(epaper_album_file_path(EPAPER_SD_MOUNT,c,c->albums[i].id,NULL,folder,sizeof(folder))) {
            snprintf(path,sizeof(path),"%s/.epaper-import-%s.tmp",folder,k?"output":"source");
            if(!stat(path,&st) && (st.st_mode&S_IFMT)==S_IFREG && remove(path))ESP_LOGW("image-import","Cannot clean interrupted import temporary file");
        }
    }
}

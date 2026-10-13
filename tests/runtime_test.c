/* Run the production runtime against a deterministic clock/display/NVS adapter. */
#include "test_platform.h"
#include <stdlib.h>
#include <string.h>
static uint64_t clock_us=1000000000;
static unsigned refreshes,writes,checks;
#include <stdbool.h>
static bool display_fail;
typedef struct {unsigned char bytes[33000];size_t size;} blob_t;
static blob_t player_blob,config_blob;
static uint8_t dirty_byte=1;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL runtime line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
#include "../components/services/runtime.c"

const char *esp_err_to_name(esp_err_t err) {(void)err;return "mock";}
uint32_t esp_random(void) {return 123;}
int gettimeofday(struct timeval *t,void *unused)
{(void)unused;t->tv_sec=(long long)(clock_us/1000000);t->tv_usec=(long)(clock_us%1000000);return 0;}
esp_err_t nvs_flash_init_partition(const char *part) {(void)part;return ESP_OK;}
esp_err_t nvs_open_from_partition(const char *part,const char *name,int mode,nvs_handle_t *out)
{(void)part;(void)name;(void)mode;*out=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size)
{
    (void)h;blob_t *b=!strcmp(key,"player")?&player_blob:&config_blob;
    if(!b->size) return ESP_ERR_NOT_FOUND;
    if(out && *size<b->size) return ESP_ERR_NO_MEM;
    *size=b->size;if(out) memcpy(out,b->bytes,b->size);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size)
{
    (void)h;blob_t *b=!strcmp(key,"player")?&player_blob:&config_blob;
    CHECK(size<=sizeof(b->bytes));memcpy(b->bytes,data,size);b->size=size;writes++;return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *out)
{(void)h;(void)key;*out=dirty_byte;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value)
{(void)h;(void)key;dirty_byte=value;writes++;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {(void)h;return ESP_OK;}
void nvs_close(nvs_handle_t h) {(void)h;}
esp_err_t epaper_display_frame(const uint8_t *data)
{CHECK(data);refreshes++;clock_us+=30000000;return display_fail?ESP_FAIL:ESP_OK;}
static void reboot(bool deep)
{
    if(deep) epaper_album_prepare_sleep();else memset(&retained,0,sizeof(retained));
    epaper_catalog_free(catalog);free(catalog);free(frame);catalog=NULL;frame=NULL;
    initialized=false;dirty=false;automatic_stopped=false;deadline=retry_at=0;
    CHECK(epaper_album_start(deep)==ESP_OK);
}
static void text_file(const char *path,const char *text)
{FILE *f=fopen(path,"wb");CHECK(f);CHECK(fputs(text,f)>=0);CHECK(!fclose(f));}
static void copy_file(const char *source,const char *destination)
{
    FILE *in=fopen(source,"rb"),*out=fopen(destination,"wb");CHECK(in && out);
    char buffer[8192];size_t n;while((n=fread(buffer,1,sizeof(buffer),in))) CHECK(fwrite(buffer,1,n,out)==n);
    CHECK(!ferror(in));CHECK(!fclose(in));CHECK(!fclose(out));
}
int main(void)
{
    /* BMP fixtures are produced by album_test.c; each needs distinct pixels. */
    const char *paths[]={".local/test-sd/albums/a/10.bmp",".local/test-sd/albums/b/1.bmp"};
    for(unsigned i=0;i<2;i++) {
        FILE *f=fopen(paths[i],"r+b");CHECK(f);CHECK(!fseek(f,54+3*(100+(long)i),SEEK_SET));
        const uint8_t p[3]={0,0,0};CHECK(fwrite(p,1,3,f)==3);fclose(f);
    }
    text_file(".local/test-sd/config.json","{\"schema_version\":1,\"album\":{\"root\":\"/albums\",\"manifest\":\"albums.json\"}}");
    text_file(".local/test-sd/albums/albums.json","{\"schema_version\":1,\"playback\":{\"default_interval_seconds\":120},\"albums\":[{\"id\":\"a\",\"directory\":\"a\"},{\"id\":\"b\",\"directory\":\"b\"}]}");
    CHECK(epaper_album_start(false)==ESP_OK);CHECK(refreshes==1 && !dirty_byte);
    CHECK(deadline==clock_us+120000000);CHECK(!epaper_album_due());
    epaper_album_status_t status;epaper_album_status(&status);
    CHECK(status.ready && status.photos==5 && status.albums==2);
    CHECK(!strcmp(status.current_album,live.current_id) && !strcmp(status.current_photo,live.current_path));
    CHECK(status.interval==120 && status.next_us==120000000);
    /* First NTP step from uptime to Unix time must preserve the remaining interval. */
    uint64_t remaining=epaper_album_sleep_us();
    int64_t step=1700000000000000LL;
    clock_us+=(uint64_t)step;epaper_album_clock_adjust(step);
    CHECK(epaper_album_sleep_us()==remaining && !epaper_album_due());
    epaper_album_prepare_sleep();uint64_t retained_before=retained.deadline;
    step=-5000000;clock_us=(uint64_t)((int64_t)clock_us+step);epaper_album_clock_adjust(step);
    CHECK(epaper_album_sleep_us()==remaining);
    CHECK(retained.deadline==retained_before-5000000);
    uint64_t original=deadline;clock_us+=20000000;unsigned before=writes;
    reboot(true);CHECK(deadline==original && refreshes==1);CHECK(writes==before);
    clock_us=original;CHECK(epaper_album_due());CHECK(epaper_album_next()==ESP_OK);
    CHECK(live.current_album==0 && live.current_photo==1 && refreshes==2);
    CHECK(deadline==clock_us+120000000);
    clock_us+=10000000;original=deadline;epaper_player_t saved=live;
    display_fail=true;CHECK(epaper_album_next()==ESP_FAIL);
    CHECK(!memcmp(&live,&saved,sizeof(live)));CHECK(deadline==original && dirty_byte==1);
    CHECK(!epaper_album_due());display_fail=false;
    reboot(false);CHECK(live.current_photo==saved.current_photo && dirty_byte==0);
    CHECK(deadline==clock_us+120000000);
    clock_us+=50000000;original=deadline;reboot(false);
    CHECK(deadline==clock_us+120000000 && deadline!=original); /* cold resets full interval */
    before=refreshes;clock_us+=1000000;reboot(true);CHECK(refreshes==before);
    /* A dirty panel on deep wake must redraw and start a new full interval. */
    dirty_byte=1;original=deadline;clock_us+=1000000;reboot(true);
    CHECK(refreshes==before+1 && deadline==clock_us+120000000 && deadline!=original);
    /* Invalid SD configuration falls back to previously fully validated pair. */
    text_file(".local/test-sd/config.json","broken");reboot(false);CHECK(catalog->total==5);
    /* Manual successful next always gets a fresh interval, including identical pixels. */
    clock_us+=110000000;CHECK(epaper_album_next()==ESP_OK);CHECK(deadline==clock_us+120000000);
    /* Web settings validate fully before disk/live changes and preserve metadata. */
    const char *global="{\"schema_version\":1,\"device\":{\"name\":\"keep-me\"},\"album\":{\"root\":\"/albums\",\"manifest\":\"albums.json\"}}";
    text_file(".local/test-sd/config.json",global);
    while(live.current_album!=0) CHECK(epaper_album_next()==ESP_OK);
    char error[160]={0};before=refreshes;
    CHECK(epaper_album_settings("{\"schema_version\":1,\"album_ids\":[\"b\"],\"interval_seconds\":300}",error,sizeof(error))==ESP_OK);
    CHECK(live.current_album==1 && catalog->total==3 && !catalog->config.albums[0].enabled);
    CHECK(catalog->config.albums[0].interval==120);
    CHECK(refreshes==before+1 && deadline==clock_us+300000000);
    char *disk=NULL;CHECK(epaper_json_read(".local/test-sd/config.json",&disk));CHECK(!strcmp(disk,global));free(disk);
    CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&disk));
    /* Independent edits must preserve the other setting, including mixed intervals. */
    char *partial_output=NULL;epaper_album_config_t partial;
    CHECK(epaper_album_edit(global,disk,"{\"schema_version\":1,\"album_ids\":[\"a\",\"b\"]}",&partial_output,&partial,error,sizeof(error)));
    CHECK(partial.albums[0].enabled && partial.albums[1].enabled);
    CHECK(partial.albums[0].interval==120 && partial.albums[1].interval==300);free(partial_output);partial_output=NULL;
    CHECK(epaper_album_edit(global,disk,"{\"schema_version\":1,\"interval_seconds\":400}",&partial_output,&partial,error,sizeof(error)));
    CHECK(!partial.albums[0].enabled && partial.albums[1].enabled);
    CHECK(partial.albums[0].interval==120 && partial.albums[1].interval==400);free(partial_output);
    const char *invalid[]={
        "{\"schema_version\":1}",
        "{\"schema_version\":1,\"album_ids\":[],\"interval_seconds\":300}",
        "{\"schema_version\":1,\"album_ids\":[\"unknown\"],\"interval_seconds\":300}",
        "{\"schema_version\":1,\"album_ids\":[\"a\",\"a\"],\"interval_seconds\":300}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":119}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":604801}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":120.5}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":300,\"interval_seconds\":400}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":300,\"path\":\"/escape\"}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":300} trailing"
    };
    saved=live;original=deadline;before=refreshes;
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        error[0]=0;CHECK(epaper_album_settings(invalid[i],error,sizeof(error))!=ESP_OK);CHECK(error[0]);
        CHECK(!memcmp(&saved,&live,sizeof(live)) && deadline==original && refreshes==before);
        char *unchanged=NULL;CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&unchanged));
        CHECK(!strcmp(unchanged,disk));free(unchanged);
    }
    free(disk);error[0]=0;
    CHECK(epaper_album_settings("{\"schema_version\":1,\"album_ids\":[\"a\",\"b\"],\"interval_seconds\":600}",error,sizeof(error))==ESP_OK);
    CHECK(refreshes==before && !strcmp(live.current_id,"b") && catalog->total==5);
    CHECK(deadline==clock_us+600000000 && epaper_album_configuration()->albums[0].interval==600);
    clock_us+=10000000;original=deadline;reboot(true);CHECK(deadline==original && refreshes==before);
    /* Simulate interruption between old->backup and temp->main renames. */
    CHECK(!remove(".local/test-sd/albums/albums.json.web-bak"));
    CHECK(!rename(".local/test-sd/albums/albums.json",".local/test-sd/albums/albums.json.web-bak"));
    reboot(false);CHECK(catalog->total==5 && catalog->config.albums[0].interval==600);
    CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&disk));free(disk);
    /* Metadata not controlled by the page survives the manifest edit. */
    epaper_album_config_t *edited=malloc(sizeof(*edited));CHECK(edited);char *output=NULL;
    CHECK(epaper_album_edit(global,"{\"schema_version\":1,\"playback\":{\"order\":\"shuffle\"},\"albums\":[{\"id\":\"a\",\"name\":\"KEEP\",\"directory\":\"a\"}]}",
        "{\"schema_version\":1,\"album_ids\":[\"a\"],\"interval_seconds\":604800}",&output,edited,error,sizeof(error)));
    CHECK(strstr(output,"KEEP") && edited->shuffle && edited->albums[0].interval==604800);free(output);free(edited);
    /* File mutations update indexes without resetting the currently displayed photo's timer. */
    before=refreshes;original=deadline;clock_us+=10000000;
    copy_file(".local/test-sd/albums/a/2.BMP",".local/test-sd/albums/b/4.bmp");
    CHECK(epaper_album_rescan()==ESP_OK && catalog->total==6);
    CHECK(refreshes==before && deadline==original && !strcmp(live.current_id,"b"));
    CHECK(!remove(".local/test-sd/albums/b/4.bmp"));
    CHECK(epaper_album_rescan()==ESP_OK && catalog->total==5 && deadline==original);
    char current[EPAPER_PATH_MAX];snprintf(current,sizeof(current),"%s%s",EPAPER_SD_MOUNT,live.current_path);
    CHECK(!remove(current));CHECK(epaper_album_rescan()==ESP_OK && catalog->total==4);
    CHECK(refreshes==before+1 && deadline==clock_us+600000000 && !strcmp(live.current_id,"a"));
    before=refreshes;original=deadline;dirty=dirty_byte=1;
    CHECK(epaper_album_rescan()==ESP_OK && dirty && dirty_byte==1 && refreshes==before && deadline==original);
    dirty=false;dirty_byte=0;
    /* Time edits update only root time.sync fields, preserving playback and secrets. */
    const char *ntp_global="{\"schema_version\":1,\"device\":{\"name\":\"keep-time-name\"},\"album\":{\"root\":\"/albums\",\"manifest\":\"albums.json\"},\"network\":{\"wifi_enabled\":true,\"ssid\":\"test-network\",\"password\":\"test-only-password\"},\"time\":{\"rtc_storage\":\"utc\",\"sync\":{\"mode\":\"ntp\",\"servers\":[\"ntp.aliyun.com\"],\"interval_seconds\":86400,\"retry_seconds\":900,\"connect_timeout_seconds\":17,\"sync_timeout_seconds\":23}}}";
    text_file(".local/test-sd/config.json",ntp_global);reboot(false);
    saved=live;original=deadline;before=refreshes;
    char *old_manifest=NULL;CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&old_manifest));
    const char *time_request="{\"schema_version\":1,\"servers\":[\"ntp2.aliyun.com\",\"ntp.aliyun.com\"],\"interval_seconds\":43200,\"retry_seconds\":600}";
    error[0]=0;CHECK(epaper_album_time_settings(time_request,error,sizeof(error))==ESP_OK);
    CHECK(!memcmp(&saved,&live,sizeof(live)) && deadline==original && refreshes==before);
    CHECK(catalog->config.time.interval==43200 && catalog->config.time.retry==600 && catalog->config.time.server_count==2);
    CHECK(catalog->config.time.connect_timeout==17 && catalog->config.time.sync_timeout==23);
    CHECK(!strcmp(catalog->config.time.password,"test-only-password"));
    CHECK(epaper_json_read(".local/test-sd/config.json",&disk));CHECK(strstr(disk,"keep-time-name"));
    char *new_manifest=NULL;CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&new_manifest));
    CHECK(!strcmp(old_manifest,new_manifest));free(old_manifest);free(new_manifest);
    const char *bad_time[]={
        "{\"schema_version\":1,\"servers\":[],\"interval_seconds\":43200,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"ntp.aliyun.com\"],\"interval_seconds\":3599,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"ntp.aliyun.com\"],\"interval_seconds\":604801,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"ntp.aliyun.com\"],\"interval_seconds\":43200.5,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"ntp.aliyun.com\"],\"interval_seconds\":43200,\"retry_seconds\":59}",
        "{\"schema_version\":1,\"servers\":[\"ntp.aliyun.com\"],\"interval_seconds\":43200,\"retry_seconds\":86401}",
        "{\"schema_version\":1,\"servers\":[\"a\",\"b\",\"c\",\"d\"],\"interval_seconds\":43200,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"a\",\"a\"],\"interval_seconds\":43200,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"https://ntp.aliyun.com\"],\"interval_seconds\":43200,\"retry_seconds\":600}",
        "{\"schema_version\":1,\"servers\":[\"a\"],\"interval_seconds\":43200,\"retry_seconds\":600,\"retry_seconds\":700}",
        "{\"schema_version\":1,\"servers\":[\"a\"],\"interval_seconds\":43200,\"retry_seconds\":600,\"password\":\"changed\"}"
    };
    for(unsigned i=0;i<sizeof(bad_time)/sizeof(bad_time[0]);i++) {
        error[0]=0;CHECK(epaper_album_time_settings(bad_time[i],error,sizeof(error))!=ESP_OK);CHECK(error[0]);
        char *unchanged=NULL;CHECK(epaper_json_read(".local/test-sd/config.json",&unchanged));CHECK(!strcmp(disk,unchanged));free(unchanged);
        CHECK(!memcmp(&saved,&live,sizeof(live)) && deadline==original && refreshes==before);
    }
    free(disk);
    clock_us+=1000000;reboot(true);CHECK(catalog->config.time.interval==43200 && deadline==original && refreshes==before);
    CHECK(!remove(".local/test-sd/config.json.web-bak"));
    CHECK(!rename(".local/test-sd/config.json",".local/test-sd/config.json.web-bak"));
    reboot(false);CHECK(catalog->config.time.retry==600);CHECK(epaper_json_read(".local/test-sd/config.json",&disk));free(disk);
    saved=live;original=deadline;before=refreshes;
    error[0]=0;CHECK(epaper_album_wifi_settings("{\"schema_version\":1,\"ssid\":\"new-test-network\"}",error,sizeof(error))==ESP_OK);
    CHECK(!strcmp(catalog->config.time.ssid,"new-test-network") && !strcmp(catalog->config.time.password,"test-only-password"));
    CHECK(catalog->config.time.interval==43200 && catalog->config.time.retry==600 && catalog->config.time.sync_timeout==23);
    CHECK(!memcmp(&saved,&live,sizeof(live)) && deadline==original && refreshes==before);
    CHECK(epaper_album_wifi_settings("{\"schema_version\":1,\"ssid\":\"new-test-network\",\"password\":\"test-next-password\"}",error,sizeof(error))==ESP_OK);
    CHECK(!strcmp(catalog->config.time.password,"test-next-password"));
    CHECK(epaper_json_read(".local/test-sd/config.json",&disk));CHECK(strstr(disk,"keep-time-name"));
    const char *bad_wifi[]={
        "{\"schema_version\":1,\"ssid\":\"\"}",
        "{\"schema_version\":1,\"ssid\":\"12345678901234567890123456789012345\"}",
        "{\"schema_version\":1,\"ssid\":\"测试网络测试网络测试网络\"}",
        "{\"schema_version\":1,\"ssid\":\"test\",\"password\":\"short\"}",
        "{\"schema_version\":1,\"ssid\":\"test\",\"password\":false}",
        "{\"schema_version\":1,\"ssid\":\"test\",\"password\":\"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz\"}",
        "{\"schema_version\":1,\"ssid\":\"test\",\"ssid\":\"other\"}",
        "{\"schema_version\":1,\"ssid\":\"test\",\"interval_seconds\":3600}"
    };
    for(unsigned i=0;i<sizeof(bad_wifi)/sizeof(bad_wifi[0]);i++) {
        error[0]=0;CHECK(epaper_album_wifi_settings(bad_wifi[i],error,sizeof(error))!=ESP_OK);CHECK(error[0]);
        char *unchanged=NULL;CHECK(epaper_json_read(".local/test-sd/config.json",&unchanged));CHECK(!strcmp(disk,unchanged));free(unchanged);
        CHECK(!memcmp(&saved,&live,sizeof(live)) && deadline==original && refreshes==before);
    }
    free(disk);
    CHECK(epaper_album_wifi_settings("{\"schema_version\":1,\"ssid\":\"test-open-network\",\"password\":\"\"}",error,sizeof(error))==ESP_OK);
    CHECK(!catalog->config.time.password[0]);reboot(true);
    CHECK(!strcmp(catalog->config.time.ssid,"test-open-network") && !catalog->config.time.password[0]);
    CHECK(deadline==original && refreshes==before);
    CHECK(epaper_album_wifi_settings("{\"schema_version\":1,\"ssid\":\"test-network\",\"password\":\"test-only-password\"}",error,sizeof(error))==ESP_OK);
    CHECK(epaper_json_read(".local/test-sd/config.json",&disk));
    CHECK(epaper_album_wifi_restore_root(global)==ESP_OK && !catalog->config.time.enabled);
    CHECK(epaper_album_wifi_restore_root(disk)==ESP_OK && catalog->config.time.enabled);free(disk);
    saved=live;before=refreshes;
    CHECK(epaper_album_mark_display_dirty()==ESP_OK && dirty_byte==1);
    display_fail=true;CHECK(epaper_album_restore_current()!=ESP_OK && dirty_byte==1);
    CHECK(!memcmp(&saved,&live,sizeof(live)));
    display_fail=false;CHECK(epaper_album_restore_current()==ESP_OK && dirty_byte==0);
    CHECK(!strcmp(saved.current_path,live.current_path) && refreshes==before+2);
    before=refreshes;
    copy_file(".local/test-sd/albums/a/2.BMP",".local/test-sd/upload-fixture.bmp");
    CHECK(!remove(".local/test-sd/albums/a/2.BMP"));CHECK(!remove(".local/test-sd/albums/a/10.bmp"));
    CHECK(!remove(".local/test-sd/albums/b/2.bmp"));CHECK(!remove(".local/test-sd/albums/b/3.bmp"));
    CHECK(epaper_album_rescan()==ESP_OK && catalog->total==0 && automatic_stopped && !epaper_album_due());
    CHECK(refreshes==before); /* Last pixels remain when the last indexed file is removed. */
    copy_file(".local/test-sd/upload-fixture.bmp",".local/test-sd/albums/a/30.bmp");
    CHECK(epaper_album_rescan()==ESP_OK && catalog->total==1 && !automatic_stopped);
    CHECK(refreshes==before+1 && deadline==clock_us+600000000);
    CHECK(!remove(".local/test-sd/albums/a/30.bmp"));
    CHECK(epaper_album_rescan()==ESP_OK);
    before=refreshes;original=deadline;
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"world-test\",\"enabled\":false}",false,error,sizeof(error))==ESP_OK);
    CHECK(catalog->config.album_count==3 && refreshes==before && deadline==original);
    char *management_before=NULL;CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&management_before));
    text_file(".local/test-sd/albums/world-test/keep.txt","Keep non-photo files");
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"world-test\"}",true,error,sizeof(error))!=ESP_OK);
    char *management_after=NULL;CHECK(epaper_json_read(".local/test-sd/albums/albums.json",&management_after));
    CHECK(!strcmp(management_before,management_after));free(management_before);free(management_after);
    CHECK(!remove(".local/test-sd/albums/world-test/keep.txt"));
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"world-test\"}",true,error,sizeof(error))==ESP_OK);
    CHECK(catalog->config.album_count==2 && refreshes==before);
    CHECK(!remove(".local/test-sd/albums/a/bad.bmp"));
    CHECK(!remove(".local/test-sd/albums/a/nested/1.bmp"));CHECK(!remove_directory(".local/test-sd/albums/a/nested"));
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"a\"}",true,error,sizeof(error))==ESP_OK);
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"b\"}",true,error,sizeof(error))==ESP_OK);
    CHECK(catalog->config.album_count==0 && automatic_stopped && !epaper_album_due());
    epaper_catalog_free(catalog);free(catalog);free(frame);catalog=NULL;frame=NULL;initialized=false;
    CHECK(epaper_album_start(false)==ESP_ERR_NOT_FOUND);
    CHECK(initialized && catalog->config.album_count==0 && automatic_stopped);
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"world-new\",\"enabled\":false}",false,error,sizeof(error))==ESP_OK);
    copy_file(".local/test-sd/upload-fixture.bmp",".local/test-sd/albums/world-new/001.bmp");
    before=refreshes;CHECK(epaper_album_rescan()==ESP_OK && catalog->total==0 && refreshes==before);
    CHECK(epaper_album_settings("{\"schema_version\":1,\"album_ids\":[\"world-new\"],\"interval_seconds\":900}",error,sizeof(error))==ESP_OK);
    CHECK(catalog->total==1 && refreshes==before+1 && !automatic_stopped && !strcmp(live.current_id,"world-new"));
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"world-new\"}",true,error,sizeof(error))!=ESP_OK);
    /* Beijing 22:00..06:00: persist independently and defer deadlines through sleep. */
    const uint64_t day=1767225600000000ULL,hour=3600000000ULL;
    uint64_t old_deadline=deadline;before=refreshes;
    CHECK(epaper_album_settings("{\"schema_version\":1,\"night_pause\":{\"enabled\":true,\"start\":\"22:00\",\"end\":\"06:00\"}}",error,sizeof(error))==ESP_OK);
    CHECK(catalog->config.night_enabled && deadline==old_deadline && refreshes==before);
    CHECK(epaper_album_settings("{\"schema_version\":1,\"night_pause\":{\"enabled\":true,\"start\":\"22:00\",\"end\":\"06:00\"}}",error,sizeof(error))==ESP_OK);
    CHECK(deadline==old_deadline && refreshes==before);
    CHECK(epaper_album_night_wait_us(&catalog->config,1000000)==0);
    CHECK(epaper_album_night_wait_us(&catalog->config,day+14*hour-1)==0);
    CHECK(epaper_album_night_wait_us(&catalog->config,day+14*hour)==8*hour);
    CHECK(epaper_album_night_wait_us(&catalog->config,day+22*hour-1)==1);
    CHECK(epaper_album_night_wait_us(&catalog->config,day+22*hour)==0);
    clock_us=day+14*hour;deadline=clock_us-1;retry_at=0;
    CHECK(!epaper_album_due() && epaper_album_sleep_us()==8*hour);
    epaper_album_status(&status);CHECK(status.night_paused);
    reboot(true);CHECK(!epaper_album_due() && catalog->config.night_enabled && refreshes==before);
    clock_us=day+22*hour;CHECK(epaper_album_due());
    CHECK(epaper_album_next()==ESP_OK);CHECK(!epaper_album_due());
    /* A due time inside the pause moves directly to the morning boundary. */
    clock_us=day+14*hour-60000000;deadline=day+14*hour+60000000;
    CHECK(epaper_album_sleep_us()==8*hour+60000000);
    clock_us=day+15*hour;CHECK(epaper_album_next()==ESP_OK); /* manual remains available */
    CHECK(!epaper_album_due());
    CHECK(epaper_album_settings("{\"schema_version\":1,\"night_pause\":{\"enabled\":true,\"start\":\"24:00\",\"end\":\"06:00\"}}",error,sizeof(error))!=ESP_OK);
    CHECK(epaper_album_settings("{\"schema_version\":1,\"night_pause\":{\"enabled\":true,\"start\":\"22:00\",\"end\":\"22:00\"}}",error,sizeof(error))!=ESP_OK);
    CHECK(epaper_album_settings("{\"schema_version\":1,\"night_pause\":{\"enabled\":true,\"start\":\"9:00\",\"end\":\"06:00\"}}",error,sizeof(error))!=ESP_OK);
    CHECK(epaper_album_settings("{\"schema_version\":1,\"night_pause\":{\"enabled\":false,\"start\":\"22:00\",\"end\":\"06:00\"}}",error,sizeof(error))==ESP_OK);
    deadline=clock_us-1;CHECK(epaper_album_due());
    /* Non-crossing ranges are supported; their end is exclusive. */
    catalog->config.night_enabled=true;catalog->config.night_start=540;catalog->config.night_end=1080;
    CHECK(epaper_album_night_wait_us(&catalog->config,day+hour)==9*hour);
    CHECK(epaper_album_night_wait_us(&catalog->config,day+10*hour)==0);
    catalog->config.night_enabled=false;catalog->config.night_start=1320;catalog->config.night_end=360;
    CHECK(!remove(".local/test-sd/albums/world-new/001.bmp"));CHECK(epaper_album_rescan()==ESP_OK);
    CHECK(epaper_album_manage("{\"schema_version\":1,\"id\":\"world-new\"}",true,error,sizeof(error))==ESP_OK);
    CHECK(!remove(".local/test-sd/upload-fixture.bmp"));
    epaper_catalog_free(catalog);free(catalog);free(frame);
    printf("runtime tests: %u checks passed\n",checks);return 0;
}

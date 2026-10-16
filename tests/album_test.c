#include "epaper_album.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include "cJSON.h"

static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static const char *global="{\"schema_version\":1,\"album\":{\"root\":\"/albums\",\"manifest\":\"albums.json\"}}";
static const char *manifest="{\"schema_version\":1,\"playback\":{},\"albums\":[{\"id\":\"a\",\"directory\":\"a\"},{\"id\":\"b\",\"directory\":\"b\",\"interval_seconds\":120},{\"id\":\"disabled\",\"directory\":\"off\",\"enabled\":false}]}";
static void put32(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i));}
static void bmp(FILE *f,bool portrait)
{
    uint32_t w=portrait?480u:800u,h=portrait?800u:480u;
    uint8_t header[54]={0};header[0]='B';header[1]='M';put32(header+2,54+w*h*3);
    put32(header+10,54);put32(header+14,40);put32(header+18,w);put32(header+22,h);
    header[26]=1;header[28]=24;put32(header+34,w*h*3);
    CHECK(fwrite(header,1,54,f)==54);
    for(uint32_t fy=0;fy<h;fy++) for(uint32_t x=0;x<w;x++) {
        uint32_t y=h-1-fy;uint8_t pixel[3]={255,255,255};
        if(!x && !y) {pixel[0]=0;pixel[1]=0;pixel[2]=255;}
        else if(x==w-1 && !y) {pixel[0]=255;pixel[1]=0;pixel[2]=0;}
        else if(!x && y==h-1) {pixel[0]=0;pixel[1]=255;pixel[2]=0;}
        else if(x==w-1 && y==h-1) {pixel[0]=0;pixel[1]=255;pixel[2]=255;}
        if(fwrite(pixel,1,3,f)!=3) {CHECK(false);}
    }
    CHECK(!fflush(f));
}
static unsigned code(const uint8_t *frame,unsigned x,unsigned y)
{uint8_t b=frame[y*400+x/2];return x&1?b&15:b>>4;}
static void bmp_tests(void)
{
    uint8_t *frame=malloc(EPAPER_FRAME_BYTES);CHECK(frame);
    for(unsigned r=0;r<4;r++) {
        FILE *f=tmpfile();CHECK(f);bmp(f,r&1);uint32_t hash=0;
        CHECK(epaper_bmp_decode(f,(int)r*90,frame,&hash));CHECK(hash!=0);
        const unsigned expected[4][4]={{3,5,6,2},{6,3,2,5},{2,6,5,3},{5,2,3,6}};
        CHECK(code(frame,0,0)==expected[r][0]);CHECK(code(frame,799,0)==expected[r][1]);
        CHECK(code(frame,0,479)==expected[r][2]);CHECK(code(frame,799,479)==expected[r][3]);
        CHECK(code(frame,200,200)==1);
        CHECK(!fseek(f,54,SEEK_SET));CHECK(fputc(127,f)!=EOF);CHECK(!fflush(f));
        CHECK(!epaper_bmp_decode(f,(int)r*90,frame,&hash));
        CHECK(!fseek(f,28,SEEK_SET));CHECK(fputc(16,f)!=EOF);CHECK(!fflush(f));
        epaper_bmp_info_t info;CHECK(!epaper_bmp_inspect(f,(int)r*90,&info));fclose(f);
    }
    FILE *f=tmpfile();CHECK(f);bmp(f,false);epaper_bmp_info_t info;
    CHECK(!epaper_bmp_inspect(f,90,&info));CHECK(!epaper_bmp_inspect(f,45,&info));
    CHECK(!fseek(f,22,SEEK_SET));uint8_t negative[4];put32(negative,(uint32_t)-480);
    CHECK(fwrite(negative,1,4,f)==4);CHECK(!fflush(f));CHECK(!epaper_bmp_inspect(f,0,&info));fclose(f);
    free(frame);
    CHECK(epaper_natural_compare("2.bmp","10.bmp")<0);
    CHECK(epaper_natural_compare("9999999999999999999.bmp","10000000000000000000.bmp")<0);
    CHECK(epaper_natural_compare("02.bmp","2.bmp")!=0);
}
static void config_tests(void)
{
    epaper_album_config_t *c=malloc(sizeof(*c));CHECK(c);char error[160];
    CHECK(epaper_album_parse(global,manifest,c,error,sizeof(error)));
    CHECK(c->album_count==3 && c->albums[0].interval==900 && c->albums[1].interval==120);
    CHECK(c->sleep && c->resume && c->strategy==ALBUM_BY_ALBUM);
    cJSON *wifi=cJSON_Parse(global);
    cJSON_AddItemToObject(wifi,"network",cJSON_Parse("{\"wifi_enabled\":true,\"ssid\":\"test-ap\",\"password\":\"test-pass\"}"));
    cJSON_AddItemToObject(wifi,"time",cJSON_Parse("{\"rtc_storage\":\"utc\",\"sync\":{\"mode\":\"ntp\"}}"));
    char *json=cJSON_PrintUnformatted(wifi);
    CHECK(epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    CHECK(c->time.enabled && c->time.interval==86400 && c->time.retry==900 && c->time.server_count==3);
    cJSON *sync=cJSON_GetObjectItem(cJSON_GetObjectItem(wifi,"time"),"sync");
    cJSON_AddNumberToObject(sync,"interval_seconds",3599);
    json=cJSON_PrintUnformatted(wifi);CHECK(!epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_DeleteItemFromObject(sync,"interval_seconds");
    cJSON_AddItemToObject(sync,"servers",cJSON_Parse("[]"));
    json=cJSON_PrintUnformatted(wifi);CHECK(!epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_ReplaceItemInObject(sync,"servers",cJSON_Parse("[\"bad/server\"]"));
    json=cJSON_PrintUnformatted(wifi);CHECK(!epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_ReplaceItemInObject(sync,"servers",cJSON_Parse("[\"ntp.aliyun.com\",\"ntp1.aliyun.com\"]"));
    json=cJSON_PrintUnformatted(wifi);CHECK(epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    CHECK(c->time.server_count==2);
    cJSON_ReplaceItemInObject(cJSON_GetObjectItem(wifi,"network"),"password",cJSON_CreateString("short"));
    json=cJSON_PrintUnformatted(wifi);CHECK(!epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_ReplaceItemInObject(cJSON_GetObjectItem(wifi,"network"),"password",cJSON_CreateString(""));
    json=cJSON_PrintUnformatted(wifi);CHECK(epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_ReplaceItemInObject(cJSON_GetObjectItem(wifi,"network"),"wifi_enabled",cJSON_CreateBool(false));
    json=cJSON_PrintUnformatted(wifi);CHECK(!epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_ReplaceItemInObject(wifi,"time",cJSON_CreateNumber(1));
    json=cJSON_PrintUnformatted(wifi);CHECK(!epaper_album_parse(json,manifest,c,error,sizeof(error)));free(json);
    cJSON_Delete(wifi);
    const char *bad[]={
        "{\"schema_version\":1,\"playback\":{\"default_interval_seconds\":119},\"albums\":[]}",
        "{\"schema_version\":1,\"playback\":{\"default_interval_seconds\":120.5},\"albums\":[]}",
        "{\"schema_version\":1,\"playback\":{\"recursive\":true},\"albums\":[]}",
        "{\"schema_version\":1,\"playback\":{},\"albums\":[{\"id\":\"a\",\"directory\":\"../x\"}]}",
        "{\"schema_version\":1,\"playback\":{},\"albums\":[{\"id\":\"a\",\"directory\":\"/x\"}]}",
        "{\"schema_version\":1,\"playback\":{},\"albums\":[{\"id\":\"a\",\"directory\":\"a\"},{\"id\":\"a\",\"directory\":\"b\"}]}",
        "{\"schema_version\":1,\"schema_version\":1,\"playback\":{},\"albums\":[]}",
        "{\"schema_version\":2,\"playback\":{},\"albums\":[]}",
        "{\"schema_version\":1,\"playback\":{},\"albums\":[]} trailing",
        "{\"schema_version\":1,\"playback\":{\"enabled\":1},\"albums\":[]}",
        "{\"schema_version\":1,\"playback\":{},\"albums\":[{\"id\":\"a\",\"directory\":\"a\\u0000/../x\"}]}"
    };
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) CHECK(!epaper_album_parse(global,bad[i],c,error,sizeof(error)));
    CHECK(epaper_album_parse("{\"schema_version\":1,\"album\":{\"root\":\"/albums\",\"manifest\":\"albums.json\"},\"announcement\":{\"enabled\":true}}",manifest,c,error,sizeof(error)));
    CHECK(!epaper_album_parse("{\"schema_version\":1,\"album\":{\"root\":\"/albums\",\"manifest\":\"albums.json\"},\"power\":{\"low_battery_policy\":\"suspend_refresh_and_auto_audio\"}}",manifest,c,error,sizeof(error)));
    char deep[256]="{\"schema_version\":1,\"x\":";for(unsigned i=0;i<40;i++) strcat(deep,"[");
    strcat(deep,"0");for(unsigned i=0;i<40;i++) strcat(deep,"]");strcat(deep,"}");
    CHECK(!epaper_album_parse(deep,manifest,c,error,sizeof(error)));free(c);
}
static void fixture(const char *path)
{FILE *f=fopen(path,"wb");CHECK(f);bmp(f,false);CHECK(!fclose(f));}
static void next(epaper_catalog_t *c,epaper_player_t *s,unsigned want_a,unsigned want_p)
{unsigned a,p;CHECK(epaper_player_pick(c,s,&a,&p));CHECK(a==want_a && p==want_p);epaper_player_commit(c,s,a,p,42);CHECK(epaper_player_valid(c,s));}
static void player_tests(void)
{
    remove(".local/test-sd/albums/a/01.bmp");remove(".local/test-sd/albums/a/02.bmp");
    remove(".local/test-sd/albums/off/01.bmp");remove(".local/test-sd/albums/off/02.bmp");remove(".local/test-sd/albums/off/bad.bmp");
    remove(".local/test-sd/albums/a/30.bmp");remove(".local/test-sd/albums/b/4.bmp");
    _mkdir(".local/test-sd");_mkdir(".local/test-sd/albums");_mkdir(".local/test-sd/albums/a");
    _mkdir(".local/test-sd/albums/b");_mkdir(".local/test-sd/albums/off");_mkdir(".local/test-sd/albums/a/nested");
    fixture(".local/test-sd/albums/a/10.bmp");fixture(".local/test-sd/albums/a/2.BMP");
    fixture(".local/test-sd/albums/b/1.bmp");fixture(".local/test-sd/albums/b/2.bmp");fixture(".local/test-sd/albums/b/3.bmp");
    fixture(".local/test-sd/albums/off/1.bmp");fixture(".local/test-sd/albums/a/nested/1.bmp");
    FILE *f=fopen(".local/test-sd/albums/a/bad.bmp","wb");CHECK(f);fputs("bad",f);fclose(f);
    epaper_album_config_t *config=malloc(sizeof(*config));epaper_catalog_t *c=malloc(sizeof(*c));CHECK(config && c);
    char error[160];CHECK(epaper_album_parse(global,manifest,config,error,sizeof(error)));
    CHECK(epaper_catalog_scan(".local/test-sd",config,c,error,sizeof(error)));
    CHECK(c->total==5 && c->albums[0].count==2 && c->albums[2].count==0);
    CHECK(!strcmp(c->albums[0].photos[0].name,"2.BMP"));
    /* Direct selection follows the current ordered/shuffled deck. */
    epaper_player_t chosen;epaper_strategy_t strategy=c->config.strategy;bool shuffle=c->config.shuffle;
    for(unsigned shuffled=0;shuffled<2;shuffled++)for(unsigned seed=1;seed<=20;seed++)for(unsigned selected=0;selected<2;selected++) {
        c->config.shuffle=shuffled!=0;c->config.strategy=ALBUM_BY_ALBUM;epaper_player_init(c,&chosen,seed);
        CHECK(epaper_player_select(c,&chosen,0,selected));CHECK(chosen.cursor[0]>=1 && chosen.cursor[0]<=2);
        CHECK(c->albums[0].deck[chosen.cursor[0]-1]==selected);
        unsigned expected=chosen.cursor[0]<2?c->albums[0].deck[chosen.cursor[0]]:UINT32_MAX;
        epaper_player_commit(c,&chosen,0,selected,42);CHECK(epaper_player_valid(c,&chosen));
        unsigned album,photo;CHECK(epaper_player_pick(c,&chosen,&album,&photo));
        CHECK(expected==UINT32_MAX?album==1:album==0 && photo==expected);
    }
    c->config.shuffle=false;c->config.strategy=ALBUM_ROUND_ROBIN;epaper_player_init(c,&chosen,123);
    CHECK(epaper_player_select(c,&chosen,0,0));CHECK(chosen.active==1);
    epaper_player_t unchanged=chosen;CHECK(!epaper_player_select(c,&chosen,2,0));CHECK(!epaper_player_select(c,&chosen,0,2));CHECK(!memcmp(&chosen,&unchanged,sizeof(chosen)));
    c->config.shuffle=shuffle;c->config.strategy=strategy;

    epaper_player_t s;epaper_player_init(c,&s,123);
    next(c,&s,0,0);next(c,&s,0,1);next(c,&s,1,0);next(c,&s,1,1);next(c,&s,1,2);next(c,&s,0,0);
    epaper_player_t saved=s,restored;epaper_player_init(c,&restored,456);
    CHECK(epaper_player_restore(c,&restored,&saved));next(c,&restored,0,1);
    epaper_player_t trial=s;next(c,&trial,0,1);trial=s;next(c,&trial,0,1); /* failed trial never advances live */
    c->fingerprint++;epaper_player_init(c,&restored,456);CHECK(epaper_player_restore(c,&restored,&saved));
    CHECK(!strcmp(restored.current_path,saved.current_path));next(c,&restored,0,1);
    strcpy(saved.current_path,"/albums/a/deleted.bmp");epaper_player_init(c,&restored,456);
    CHECK(!epaper_player_restore(c,&restored,&saved));next(c,&restored,0,0);
    c->config.strategy=ALBUM_ROUND_ROBIN;epaper_player_init(c,&s,123);
    next(c,&s,0,0);next(c,&s,1,0);next(c,&s,0,1);next(c,&s,1,1);next(c,&s,0,0);next(c,&s,1,2);
    c->config.strategy=ALBUM_BY_ALBUM;c->config.shuffle=true;epaper_player_init(c,&s,123);
    unsigned previous=UINT32_MAX;
    for(unsigned cycle=0;cycle<50;cycle++) {
        unsigned seen[2]={0,0},a,p;
        for(unsigned i=0;i<2;i++) {
            CHECK(epaper_player_pick(c,&s,&a,&p));CHECK(a==0 && p<2 && !seen[p]);
            if(!i) CHECK(p!=previous);seen[p]=1;previous=p;epaper_player_commit(c,&s,a,p,42);
            saved=s;epaper_player_init(c,&restored,999);CHECK(epaper_player_restore(c,&restored,&saved));s=restored;
        }
        unsigned bseen=0;
        for(unsigned i=0;i<3;i++) {CHECK(epaper_player_pick(c,&s,&a,&p));CHECK(a==1 && !(bseen&(1u<<p)));bseen|=1u<<p;epaper_player_commit(c,&s,a,p,42);}
    }
    for(unsigned a=0;a<3;a++) for(size_t p=0;p<c->albums[a].count;p++) c->albums[a].photos[p].bad=true;
    unsigned a,p;CHECK(!epaper_player_pick(c,&s,&a,&p));
    epaper_catalog_free(c);free(c);free(config);
}
static void provision_config_tests(void)
{
    epaper_album_config_t *c=malloc(sizeof(*c));CHECK(c);char *out=NULL,error[160];
    CHECK(epaper_wifi_edit(global,manifest,"{\"schema_version\":1,\"ssid\":\"new-network\",\"password\":\"test-only-password\"}",&out,c,error,sizeof(error)));
    CHECK(c->time.enabled && !strcmp(c->time.ssid,"new-network") && c->time.server_count==3 && c->time.interval==86400);
    CHECK(strstr(out,"wifi_enabled") && !strcmp(c->root,"/albums") && c->album_count==3);free(out);out=NULL;
    CHECK(!epaper_wifi_edit(global,manifest,"{\"schema_version\":1,\"ssid\":\"new-network\"}",&out,c,error,sizeof(error)));
    CHECK(!out);
    CHECK(epaper_wifi_edit(global,manifest,"{\"schema_version\":1,\"ssid\":\"open-network\",\"password\":\"\"}",&out,c,error,sizeof(error)));
    CHECK(c->time.enabled && !c->time.password[0]);free(out);free(c);
}
static void file_tests(void)
{
    epaper_album_config_t *config=malloc(sizeof(*config));CHECK(config);char error[160],path[EPAPER_PATH_MAX];
    CHECK(epaper_album_parse(global,manifest,config,error,sizeof(error)));
    CHECK(epaper_photo_name_valid("test 01.BMP"));
    CHECK(epaper_photo_name_valid("\xe9\xa3\x8e\xe6\x99\xaf.bmp"));
    const char *bad_names[]={"../x.bmp","/x.bmp","x\\y.bmp",".hidden.bmp","x..bmp","x.jpg","x.bmp ","x:y.bmp","x\n.bmp","\xc0\xaf.bmp","\xed\xa0\x80.bmp"};
    for(unsigned i=0;i<sizeof(bad_names)/sizeof(bad_names[0]);i++) CHECK(!epaper_photo_name_valid(bad_names[i]));
    CHECK(epaper_album_file_path(".local/test-sd",config,"a","2.BMP",path,sizeof(path)));
    CHECK(!strcmp(path,".local/test-sd/albums/a/2.BMP"));
    CHECK(!epaper_album_file_path(".local/test-sd",config,"unknown","2.BMP",path,sizeof(path)));
    CHECK(!epaper_album_file_path(".local/test-sd",config,"a","../config.bmp",path,sizeof(path)));
    CHECK(!epaper_album_file_path(".local/test-sd",config,"a","2.BMP",path,5));
    epaper_file_entry_t *files=NULL;size_t count;
    FILE *temporary=fopen(".local/test-sd/albums/a/.epaper-upload.tmp","wb");CHECK(temporary);fputs("partial",temporary);fclose(temporary);
    CHECK(epaper_files_list(".local/test-sd",config,"a",&files,&count)==FILE_LIST_OK);
    CHECK(count==3 && !strcmp(files[0].name,"2.BMP") && !strcmp(files[1].name,"10.bmp"));
    CHECK(files[0].header_valid && files[1].header_valid && !files[2].header_valid);
    CHECK(files[0].bytes==1152054 && !strcmp(files[2].name,"bad.bmp"));free(files);
    CHECK(epaper_files_list(".local/test-sd",config,"disabled",&files,&count)==FILE_LIST_OK && count==1);free(files);
    CHECK(epaper_files_list(".local/test-sd",config,"unknown",&files,&count)==FILE_LIST_INVALID);
    strcpy(config->albums[0].directory,"missing");
    CHECK(epaper_files_list(".local/test-sd",config,"a",&files,&count)==FILE_LIST_NOT_FOUND);
    strcpy(config->albums[0].directory,"../escape");
    CHECK(!epaper_album_file_path(".local/test-sd",config,"a","2.BMP",path,sizeof(path)));
    CHECK(epaper_photo_validate(".local/test-sd/albums/a/2.BMP",0));
    CHECK(!epaper_photo_validate(".local/test-sd/albums/a/2.BMP",90));
    CHECK(!epaper_photo_validate(".local/test-sd/albums/a/bad.bmp",0));
    CHECK(!epaper_photo_validate(".local/test-sd/albums/a/.epaper-upload.tmp",0));
    CHECK(!remove(".local/test-sd/albums/a/.epaper-upload.tmp"));free(config);
}
static void album_management_tests(void)
{
    epaper_album_config_t *c=malloc(sizeof(*c));CHECK(c);
    char *out=NULL,error[160],directory[256];
    CHECK(epaper_album_manage_edit(global,manifest,"{\"schema_version\":1,\"id\":\"world-asia\",\"enabled\":false,\"interval_seconds\":900}",false,&out,c,directory,sizeof(directory),error,sizeof(error)));
    CHECK(c->album_count==4 && !c->albums[3].enabled && !strcmp(directory,"world-asia"));
    char *created=out;out=NULL;
    CHECK(!epaper_album_manage_edit(global,created,"{\"schema_version\":1,\"id\":\"world-asia\"}",false,&out,c,directory,sizeof(directory),error,sizeof(error)));
    CHECK(epaper_album_manage_edit(global,created,"{\"schema_version\":1,\"id\":\"world-asia\"}",true,&out,c,directory,sizeof(directory),error,sizeof(error)));
    CHECK(c->album_count==3 && !strcmp(directory,"world-asia"));free(out);free(created);out=NULL;
    const char *bad[]={"../escape","hidden/name","bad\\name","CON",""};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        char request[256];snprintf(request,sizeof(request),"{\"schema_version\":1,\"id\":\"%s\"}",bad[i]);
        CHECK(!epaper_album_manage_edit(global,manifest,request,false,&out,c,directory,sizeof(directory),error,sizeof(error)));
    }
    const char *last="{\"schema_version\":1,\"playback\":{},\"albums\":[{\"id\":\"a\",\"directory\":\"a\"}]}";
    CHECK(epaper_album_manage_edit(global,last,"{\"schema_version\":1,\"id\":\"a\"}",true,&out,c,directory,sizeof(directory),error,sizeof(error)));
    CHECK(c->album_count==0);free(out);out=NULL;
    CHECK(epaper_album_manage_edit(global,manifest,"{\"schema_version\":1,\"id\":\"cn\",\"directory\":\"武汉风景\"}",false,&out,c,directory,sizeof(directory),error,sizeof(error)));
    CHECK(!strcmp(directory,"武汉风景") && !c->albums[3].enabled);free(out);out=NULL;
    char previous[256],next[256];
    CHECK(epaper_album_rename_edit(global,manifest,"{\"schema_version\":1,\"id\":\"a\",\"directory\":\"旅行相册\"}",&out,c,previous,next,sizeof(next),error,sizeof(error)));
    CHECK(!strcmp(c->albums[0].id,"a") && !strcmp(c->albums[0].directory,"旅行相册") && c->albums[0].enabled && c->albums[0].interval==900);free(out);out=NULL;
    const char *bad_names[]={"", "../escape", "a/b", "A", "b", "off", "CON", "CON.txt", ".hidden", "trailing.", "x..y", " a", "a ", "a\\b"};
    for(unsigned i=0;i<sizeof(bad_names)/sizeof(bad_names[0]);i++) {
        cJSON *edit=cJSON_CreateObject();cJSON_AddNumberToObject(edit,"schema_version",1);cJSON_AddStringToObject(edit,"id","a");cJSON_AddStringToObject(edit,"directory",bad_names[i]);char *request=cJSON_PrintUnformatted(edit);
        CHECK(!epaper_album_rename_edit(global,manifest,request,&out,c,previous,next,sizeof(next),error,sizeof(error)));free(request);cJSON_Delete(edit);
    }
    const char *shared="{\"schema_version\":1,\"albums\":[{\"id\":\"a\",\"directory\":\"a\"},{\"id\":\"nested\",\"directory\":\"A/sub\"}]}";
    CHECK(!epaper_album_rename_edit(global,shared,"{\"schema_version\":1,\"id\":\"a\",\"directory\":\"new\"}",&out,c,previous,next,sizeof(next),error,sizeof(error)));
    CHECK(!epaper_album_manage_edit(global,manifest,"{\"schema_version\":1,\"id\":\"new\",\"directory\":\"B\"}",false,&out,c,directory,sizeof(directory),error,sizeof(error)));
    free(c);
}
int main(void)
{album_management_tests();bmp_tests();config_tests();player_tests();file_tests();provision_config_tests();printf("album tests: %u checks passed\n",checks);return 0;}

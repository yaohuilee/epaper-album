#include "epaper_album.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool fail(char *error,size_t size,const char *message)
{ if(error && size) snprintf(error,size,"%s",message); return false; }
static const cJSON *item(const cJSON *object,const char *name)
{ return cJSON_GetObjectItemCaseSensitive(object,name); }
static bool unknown(const cJSON *object,const char *names)
{
    for(const cJSON *a=object?object->child:NULL;a;a=a->next) {
        if(!a->string) continue;
        char key[128];int n=snprintf(key,sizeof(key),"|%s|",a->string);
        if(n<0 || (size_t)n>=sizeof(key) || !strstr(names,key)) return true;
    }
    return false;
}
static bool unique(const cJSON *value)
{
    for(const cJSON *a=value->child;a;a=a->next) {
        if(cJSON_IsObject(value)) {
            for(const cJSON *b=a->next;b;b=b->next)
                if(a->string && b->string && !strcmp(a->string,b->string)) return false;
        }
        if(!unique(a)) return false;
    }
    return true;
}
static cJSON *parse(const char *text)
{
    if(!text || strlen(text)>16384) return NULL;
    unsigned depth=0;bool quoted=false,escaped=false;
    for(const char *p=text;*p;p++) {
        if(quoted) {
            if(escaped) {escaped=false;continue;}
            if(*p=='\\') {
                if(!strncmp(p,"\\u0000",6)) return NULL;
                escaped=true;
            } else if(*p=='"') quoted=false;
        } else if(*p=='"') quoted=true;
        else if(*p=='{' || *p=='[') {if(++depth>32) return NULL;}
        else if(*p=='}' || *p==']') {if(!depth) return NULL;depth--;}
    }
    if(depth || quoted) return NULL;
    cJSON *root=cJSON_ParseWithOpts(text,NULL,true);
    if(!root || !cJSON_IsObject(root) || !unique(root)) { cJSON_Delete(root); return NULL; }
    const cJSON *v=item(root,"schema_version");
    if(!cJSON_IsNumber(v) || v->valuedouble!=1) { cJSON_Delete(root); return NULL; }
    return root;
}
static bool string_copy(const cJSON *object,const char *key,char *out,size_t size)
{
    const cJSON *v=item(object,key);
    if(!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring)>=size) return false;
    strcpy(out,v->valuestring);return true;
}
static bool boolean(const cJSON *object,const char *key,bool default_value,bool *out)
{
    const cJSON *v=item(object,key);
    if(!v) {*out=default_value;return true;}
    if(!cJSON_IsBool(v)) return false;
    *out=cJSON_IsTrue(v);return true;
}
static bool number(const cJSON *object,const char *key,uint32_t default_value,
                   uint32_t minimum,uint32_t maximum,uint32_t *out)
{
    const cJSON *v=item(object,key);
    if(!v) {*out=default_value;return true;}
    if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<minimum ||
       v->valuedouble>maximum || v->valuedouble!=(double)(uint32_t)v->valuedouble) return false;
    *out=(uint32_t)v->valuedouble;return true;
}
static bool safe_path(const char *path,bool absolute)
{
    if(!path || !*path || (path[0]=='/')!=absolute) return false;
    const char *p=path+(absolute?1:0),*start=p;
    if(absolute && !*p) return true;
    for(;;p++) {
        unsigned char c=(unsigned char)*p;
        if(c=='\\' || c==':' || (c && c<32)) return false;
        if(!c || c=='/') {
            size_t len=(size_t)(p-start);
            if(!len || (len==1 && start[0]=='.') || (len==2 && start[0]=='.' && start[1]=='.')) return false;
            if(!c) return true;
            start=p+1;
        }
    }
}
static bool location(const cJSON *global,char *root,size_t root_size,char *manifest,size_t manifest_size)
{
    const cJSON *album=item(global,"album");
    return cJSON_IsObject(album) && string_copy(album,"root",root,root_size) &&
        string_copy(album,"manifest",manifest,manifest_size) && safe_path(root,true) &&
        safe_path(manifest,false) && !strchr(manifest,'/');
}
bool epaper_album_location(const char *text,char *root,size_t root_size,char *manifest,
                          size_t manifest_size,char *error,size_t error_size)
{
    cJSON *global=parse(text);
    bool ok=global && location(global,root,root_size,manifest,manifest_size);
    cJSON_Delete(global);
    return ok || fail(error,error_size,"Invalid global JSON, schema or album path");
}
static bool unsupported_enabled(const cJSON *global,const char *section,const char *key)
{
    const cJSON *s=item(global,section);
    if(!s) return false;
    bool enabled;
    return !cJSON_IsObject(s) || !boolean(s,key,false,&enabled) || enabled;
}
bool epaper_album_parse(const char *text,const char *manifest_text,epaper_album_config_t *out,
                       char *error,size_t error_size)
{
    if(!out) return fail(error,error_size,"Missing output");
    cJSON *global=parse(text),*manifest=parse(manifest_text);
    epaper_album_config_t *c=calloc(1,sizeof(*c));
    bool ok=false;
    const char *reason="Invalid JSON or schema (duplicate keys are rejected)";
    if(!global || !manifest || !c) goto finish;
    reason="Invalid album root or manifest path";
    if(!location(global,c->root,sizeof(c->root),c->manifest,sizeof(c->manifest))) goto finish;
    reason="Announcements and activity are not implemented";
    if(unsupported_enabled(global,"announcement","enabled") ||
       unsupported_enabled(global,"activity","enabled")) goto finish;
    const cJSON *network=item(global,"network");
    reason="Invalid Wi-Fi or time synchronization configuration";
    if(network && !cJSON_IsObject(network)) goto finish;
    bool wifi;
    if(!boolean(network,"wifi_enabled",false,&wifi)) goto finish;
    const cJSON *time=item(global,"time");
    if(time && !cJSON_IsObject(time)) goto finish;
    const cJSON *sync=item(time,"sync");
    if(sync && !cJSON_IsObject(sync)) goto finish;
    const cJSON *mode=item(sync,"mode");
    if(mode && (!cJSON_IsString(mode) || (strcmp(mode->valuestring,"manual") && strcmp(mode->valuestring,"ntp")))) goto finish;
    c->time.enabled=mode && !strcmp(mode->valuestring,"ntp");
    if(wifi!=c->time.enabled) goto finish; /* NTP and the debug status page share Wi-Fi settings. */
    if(c->time.enabled) {
        const cJSON *password=item(network,"password"),*servers=item(sync,"servers");
        if(!string_copy(network,"ssid",c->time.ssid,sizeof(c->time.ssid)) ||
           !cJSON_IsString(password) || strlen(password->valuestring)>64 ||
           (strlen(password->valuestring)>0 && strlen(password->valuestring)<8)) goto finish;
        strcpy(c->time.password,password->valuestring);
        if(strlen(c->time.password)==64)
            for(const char *p=c->time.password;*p;p++) if(!isxdigit((unsigned char)*p)) goto finish;
        if(!number(sync,"interval_seconds",86400,3600,604800,&c->time.interval) ||
           !number(sync,"retry_seconds",900,60,86400,&c->time.retry) ||
           !number(sync,"connect_timeout_seconds",15,5,60,&c->time.connect_timeout) ||
           !number(sync,"sync_timeout_seconds",20,5,60,&c->time.sync_timeout)) goto finish;
        const cJSON *basis=item(item(global,"time"),"rtc_storage");
        if(basis && (!cJSON_IsString(basis) || strcmp(basis->valuestring,"utc"))) goto finish;
        if(servers) {
            if(!cJSON_IsArray(servers)) goto finish;
            for(const cJSON *s=servers->child;s;s=s->next) {
                if(c->time.server_count>=3 || !cJSON_IsString(s) || !s->valuestring[0] || strlen(s->valuestring)>=128) goto finish;
                for(const char *p=s->valuestring;*p;p++) if(!isalnum((unsigned char)*p) && *p!='.' && *p!='-') goto finish;
                strcpy(c->time.servers[c->time.server_count++],s->valuestring);
            }
            if(!c->time.server_count) goto finish;
        } else {
            strcpy(c->time.servers[0],"ntp.aliyun.com");strcpy(c->time.servers[1],"ntp1.aliyun.com");
            strcpy(c->time.servers[2],"ntp2.aliyun.com");c->time.server_count=3;
        }
    }
    reason="Invalid display or sleep configuration";
    const cJSON *display=item(global,"display"),*power=item(global,"power");
    if(display && !cJSON_IsObject(display)) goto finish;
    if(power && !cJSON_IsObject(power)) goto finish;
    const cJSON *battery=item(power,"low_battery_policy");
    if(battery && (!cJSON_IsString(battery) || strcmp(battery->valuestring,"none"))) {
        reason="Low-battery automation is not calibrated; use policy none in M1";goto finish;
    }
    uint32_t rotation;
    if(!number(display,"rotation_degrees",0,0,270,&rotation) || rotation%90 ||
       !boolean(power,"sleep_between_events",true,&c->sleep)) goto finish;
    c->rotation=(int)rotation;
    const cJSON *invalid=item(display,"on_invalid_image");
    if(invalid && (!cJSON_IsString(invalid) || strcmp(invalid->valuestring,"skip"))) goto finish;
    const cJSON *playback=item(manifest,"playback"),*albums=item(manifest,"albums");
    reason="Invalid playback configuration";
    if(!cJSON_IsObject(playback) || !cJSON_IsArray(albums) ||
       !boolean(playback,"enabled",true,&c->enabled) ||
       !boolean(playback,"resume_position",true,&c->resume)) goto finish;
    bool recursive;
    if(!boolean(playback,"recursive",false,&recursive) || recursive) goto finish;
    uint32_t interval;
    if(!number(playback,"default_interval_seconds",900,120,604800,&interval)) goto finish;
    const cJSON *order=item(playback,"order"),*strategy=item(playback,"album_strategy");
    if(order) {
        if(!cJSON_IsString(order)) goto finish;
        if(!strcmp(order->valuestring,"shuffle")) c->shuffle=true;
        else if(strcmp(order->valuestring,"sequential")) goto finish;
    }
    if(strategy) {
        if(!cJSON_IsString(strategy)) goto finish;
        if(!strcmp(strategy->valuestring,"round_robin")) c->strategy=ALBUM_ROUND_ROBIN;
        else if(strcmp(strategy->valuestring,"album_by_album")) goto finish;
    }
    reason="Invalid album entry, duplicate ID or unsafe directory";
    for(const cJSON *a=albums->child;a;a=a->next) {
        if(c->album_count>=EPAPER_MAX_ALBUMS || !cJSON_IsObject(a)) goto finish;
        epaper_album_definition_t *d=&c->albums[c->album_count];
        if(!string_copy(a,"id",d->id,sizeof(d->id)) ||
           !string_copy(a,"directory",d->directory,sizeof(d->directory)) || !safe_path(d->directory,false) ||
           !boolean(a,"enabled",true,&d->enabled) ||
           !number(a,"interval_seconds",interval,120,604800,&d->interval)) goto finish;
        for(const char *p=d->id;*p;p++) if(!isalnum((unsigned char)*p) && *p!='_' && *p!='-') goto finish;
        for(unsigned i=0;i<c->album_count;i++) if(!strcmp(c->albums[i].id,d->id)) goto finish;
        c->album_count++;
    }
    *out=*c;ok=true;
    if(error && error_size) {
        bool warning=unknown(global,"|schema_version|device|album|display|announcement|time|sensor|power|activity|network|") ||
            unknown(item(global,"album"),"|root|manifest|") ||
            unknown(display,"|rotation_degrees|on_invalid_image|") ||
            unknown(power,"|sleep_between_events|low_battery_policy|") ||
            unknown(manifest,"|schema_version|playback|albums|") ||
            unknown(playback,"|enabled|default_interval_seconds|order|album_strategy|recursive|resume_position|");
        for(const cJSON *a=albums->child;a;a=a->next)
            warning=warning || unknown(a,"|id|name|directory|enabled|interval_seconds|");
        snprintf(error,error_size,"%s",warning?"Unknown fields ignored; check spelling against schema":"");
    }
finish:
    cJSON_Delete(global);cJSON_Delete(manifest);free(c);
    return ok || fail(error,error_size,reason);
}

bool epaper_album_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *error,size_t error_size)
{
    *output=NULL;
    cJSON *edit=parse(request),*doc=parse(manifest);
    bool ok=false;
    const char *reason="Invalid settings JSON";
    if(!edit || !doc || unknown(edit,"|schema_version|album_ids|interval_seconds|")) goto end;
    const cJSON *ids=item(edit,"album_ids"),*interval=item(edit,"interval_seconds");
    uint32_t seconds=900;
    reason="Choose at least one album; interval must be 120..604800 whole seconds";
    if((!ids && !interval) || (ids && (!cJSON_IsArray(ids) || cJSON_GetArraySize(ids)<1 || cJSON_GetArraySize(ids)>EPAPER_MAX_ALBUMS)) ||
       (interval && !number(edit,"interval_seconds",900,120,604800,&seconds))) goto end;
    cJSON *albums=cJSON_GetObjectItemCaseSensitive(doc,"albums");
    cJSON *playback=cJSON_GetObjectItemCaseSensitive(doc,"playback");
    if(!cJSON_IsArray(albums) || !cJSON_IsObject(playback)) goto end;
    uint32_t previous_default;
    if(!number(playback,"default_interval_seconds",900,120,604800,&previous_default)) goto end;
    reason="Unknown or duplicate album ID";
    for(const cJSON *id=ids?ids->child:NULL;id;id=id->next) {
        if(!cJSON_IsString(id)) goto end;
        for(const cJSON *other=id->next;other;other=other->next)
            if(cJSON_IsString(other) && !strcmp(id->valuestring,other->valuestring)) goto end;
        bool found=false;
        for(const cJSON *a=albums->child;a;a=a->next) {
            const cJSON *key=item(a,"id");
            if(cJSON_IsString(key) && !strcmp(key->valuestring,id->valuestring)) found=true;
        }
        if(!found) goto end;
    }
    reason="Not enough memory for settings";
    if(ids) {
        cJSON_DeleteItemFromObjectCaseSensitive(playback,"enabled");
        if(!cJSON_AddBoolToObject(playback,"enabled",true)) goto end;
    }
    if(interval) {
        cJSON_DeleteItemFromObjectCaseSensitive(playback,"default_interval_seconds");
        if(!cJSON_AddNumberToObject(playback,"default_interval_seconds",seconds)) goto end;
    }
    for(cJSON *a=albums->child;a;a=a->next) {
        const cJSON *key=item(a,"id");bool selected=cJSON_IsTrue(item(a,"enabled")) || !item(a,"enabled");
        uint32_t previous_interval;
        if(!number(a,"interval_seconds",previous_default,120,604800,&previous_interval)) goto end;
        if(ids) selected=false;
        for(const cJSON *id=ids?ids->child:NULL;id;id=id->next)
            if(cJSON_IsString(key) && !strcmp(key->valuestring,id->valuestring)) selected=true;
        if(ids) {
            cJSON_DeleteItemFromObjectCaseSensitive(a,"enabled");
            if(!cJSON_AddBoolToObject(a,"enabled",selected)) goto end;
        }
        if(interval) {
            cJSON_DeleteItemFromObjectCaseSensitive(a,"interval_seconds");
            if(!cJSON_AddNumberToObject(a,"interval_seconds",selected?seconds:previous_interval)) goto end;
        }
    }
    *output=cJSON_Print(doc);
    if(!*output || strlen(*output)>16384) goto end;
    ok=epaper_album_parse(global,*output,config,error,error_size);
end:
    cJSON_Delete(edit);cJSON_Delete(doc);
    if(!ok) {free(*output);*output=NULL;return fail(error,error_size,reason);}
    return true;
}

bool epaper_album_manage_edit(const char *global,const char *manifest,const char *request,
    bool deleting,char **output,epaper_album_config_t *config,char *directory,size_t directory_size,
    char *error,size_t error_size)
{
    *output=NULL;directory[0]=0;
    cJSON *edit=parse(request),*doc=parse(manifest);bool ok=false;
    const char *reason="Invalid album management request";
    if(!edit || !doc || unknown(edit,deleting?"|schema_version|id|":"|schema_version|id|enabled|interval_seconds|")) goto end;
    char id[EPAPER_ID_MAX];
    if(!string_copy(edit,"id",id,sizeof(id))) goto end;
    for(const char *p=id;*p;p++) if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-')) goto end;
    char upper[EPAPER_ID_MAX];
    for(size_t i=0;i<=strlen(id);i++) upper[i]=(char)toupper((unsigned char)id[i]);
    if(!deleting && (!strcmp(upper,"CON") || !strcmp(upper,"PRN") || !strcmp(upper,"AUX") || !strcmp(upper,"NUL") ||
       (strlen(upper)==4 && (!strncmp(upper,"COM",3)||!strncmp(upper,"LPT",3)) && upper[3]>='1' && upper[3]<='9'))) goto end;
    cJSON *albums=cJSON_GetObjectItemCaseSensitive(doc,"albums");
    if(!cJSON_IsArray(albums)) goto end;
    int index=0,found=-1;
    for(cJSON *a=albums->child;a;a=a->next,index++) {
        const cJSON *key=item(a,"id");
        if(cJSON_IsString(key) && !strcmp(key->valuestring,id)) found=index;
    }
    if(deleting) {
        reason="Album does not exist";if(found<0) goto end;
        const cJSON *entry=cJSON_GetArrayItem(albums,found);
        reason="Only a single safe album directory can be removed";
        if(!string_copy(entry,"directory",directory,directory_size) || strchr(directory,'/') || strchr(directory,'\\') || directory[0]=='.') goto end;
        for(cJSON *a=albums->child;a;a=a->next) if(a!=entry) {
            const cJSON *dir=item(a,"directory");
            if(cJSON_IsString(dir) && (!strcmp(dir->valuestring,directory) ||
               (!strncmp(dir->valuestring,directory,strlen(directory)) && dir->valuestring[strlen(directory)]=='/'))) {
                reason="Directory is shared by another configured album";goto end;
            }
        }
        cJSON_DeleteItemFromArray(albums,found);
    } else {
        reason="Album limit reached or ID/directory already exists";
        if(found>=0 || cJSON_GetArraySize(albums)>=EPAPER_MAX_ALBUMS) goto end;
        for(cJSON *a=albums->child;a;a=a->next) {
            const cJSON *dir=item(a,"directory");
            if(cJSON_IsString(dir) && !strcmp(dir->valuestring,id)) goto end;
        }
        char filename[EPAPER_ID_MAX+5];snprintf(filename,sizeof(filename),"%s.bmp",id);
        if(!epaper_photo_name_valid(filename) || strlen(id)>=directory_size) goto end;
        bool enabled;uint32_t interval;
        reason="Invalid enabled flag or interval (120..604800 seconds)";
        if(!boolean(edit,"enabled",false,&enabled) || !number(edit,"interval_seconds",900,120,604800,&interval)) goto end;
        cJSON *entry=cJSON_CreateObject();if(!entry) goto end;
        if(!cJSON_AddStringToObject(entry,"id",id) || !cJSON_AddStringToObject(entry,"directory",id) ||
           !cJSON_AddBoolToObject(entry,"enabled",enabled) || !cJSON_AddNumberToObject(entry,"interval_seconds",interval) ||
           !cJSON_AddItemToArray(albums,entry)) {cJSON_Delete(entry);goto end;}
        strcpy(directory,id);
    }
    *output=cJSON_Print(doc);
    if(!*output || strlen(*output)>16384) goto end;
    ok=epaper_album_parse(global,*output,config,error,error_size);
end:
    cJSON_Delete(edit);cJSON_Delete(doc);
    if(!ok) {free(*output);*output=NULL;return fail(error,error_size,reason);}
    return true;
}

bool epaper_time_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *error,size_t error_size)
{
    *output=NULL;
    cJSON *edit=parse(request),*doc=parse(global);bool ok=false;
    const char *reason="Invalid time settings JSON";
    if(!edit || !doc || unknown(edit,"|schema_version|servers|interval_seconds|retry_seconds|")) goto end;
    const cJSON *servers=item(edit,"servers");uint32_t interval,retry;
    reason="Use 1..3 servers, interval 3600..604800 and retry 60..86400 whole seconds";
    if(!cJSON_IsArray(servers) || cJSON_GetArraySize(servers)<1 || cJSON_GetArraySize(servers)>3 ||
       !item(edit,"interval_seconds") || !item(edit,"retry_seconds") ||
       !number(edit,"interval_seconds",86400,3600,604800,&interval) ||
       !number(edit,"retry_seconds",900,60,86400,&retry)) goto end;
    for(const cJSON *s=servers->child;s;s=s->next) {
        if(!cJSON_IsString(s) || !s->valuestring[0] || strlen(s->valuestring)>=128) goto end;
        for(const char *p=s->valuestring;*p;p++) if(!isalnum((unsigned char)*p) && *p!='.' && *p!='-') goto end;
        for(const cJSON *other=s->next;other;other=other->next)
            if(cJSON_IsString(other) && !strcmp(s->valuestring,other->valuestring)) goto end;
    }
    cJSON *sync=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(doc,"time"),"sync");
    reason="Time synchronization must already be configured";
    if(!cJSON_IsObject(sync)) goto end;
    cJSON *copy=cJSON_Duplicate(servers,true);if(!copy) goto end;
    cJSON_DeleteItemFromObjectCaseSensitive(sync,"servers");
    if(!cJSON_AddItemToObject(sync,"servers",copy)) {cJSON_Delete(copy);goto end;}
    cJSON_DeleteItemFromObjectCaseSensitive(sync,"interval_seconds");
    cJSON_DeleteItemFromObjectCaseSensitive(sync,"retry_seconds");
    if(!cJSON_AddNumberToObject(sync,"interval_seconds",interval) || !cJSON_AddNumberToObject(sync,"retry_seconds",retry)) goto end;
    *output=cJSON_Print(doc);
    if(!*output || strlen(*output)>16384) goto end;
    ok=epaper_album_parse(*output,manifest,config,error,error_size) && config->time.enabled;
end:
    cJSON_Delete(edit);cJSON_Delete(doc);
    if(!ok) {free(*output);*output=NULL;return fail(error,error_size,reason);}
    return true;
}

bool epaper_wifi_edit(const char *global,const char *manifest,const char *request,
    char **output,epaper_album_config_t *config,char *error,size_t error_size)
{
    *output=NULL;cJSON *edit=parse(request),*doc=parse(global);bool ok=false;
    const char *reason="Invalid Wi-Fi settings JSON";
    if(!edit || !doc || unknown(edit,"|schema_version|ssid|password|")) goto end;
    const cJSON *ssid=item(edit,"ssid"),*password=item(edit,"password");
    reason="SSID must be 1..32 bytes; password must be empty, 8..63 bytes or 64 hexadecimal characters";
    if(!cJSON_IsString(ssid) || !ssid->valuestring[0] || strlen(ssid->valuestring)>32 ||
       (password && !cJSON_IsString(password))) goto end;
    if(password) {
        size_t n=strlen(password->valuestring);
        if(n>64 || (n>0 && n<8)) goto end;
        if(n==64) for(const char *p=password->valuestring;*p;p++) if(!isxdigit((unsigned char)*p)) goto end;
    }
    cJSON *network=cJSON_GetObjectItemCaseSensitive(doc,"network");
    if(!network) network=cJSON_AddObjectToObject(doc,"network");
    if(!cJSON_IsObject(network)) goto end;
    cJSON *time=cJSON_GetObjectItemCaseSensitive(doc,"time");
    if(!time) time=cJSON_AddObjectToObject(doc,"time");
    if(!cJSON_IsObject(time)) goto end;
    cJSON *sync=cJSON_GetObjectItemCaseSensitive(time,"sync");
    if(!sync) sync=cJSON_AddObjectToObject(time,"sync");
    if(!cJSON_IsObject(sync)) goto end;
    cJSON_DeleteItemFromObjectCaseSensitive(network,"wifi_enabled");
    cJSON_DeleteItemFromObjectCaseSensitive(sync,"mode");
    if(!cJSON_AddBoolToObject(network,"wifi_enabled",true) || !cJSON_AddStringToObject(sync,"mode","ntp")) goto end;
    if(!password && !cJSON_IsString(item(network,"password"))) goto end;
    cJSON_DeleteItemFromObjectCaseSensitive(network,"ssid");
    if(!cJSON_AddStringToObject(network,"ssid",ssid->valuestring)) goto end;
    if(password) {
        cJSON_DeleteItemFromObjectCaseSensitive(network,"password");
        if(!cJSON_AddStringToObject(network,"password",password->valuestring)) goto end;
    }
    *output=cJSON_Print(doc);if(!*output || strlen(*output)>16384) goto end;
    ok=epaper_album_parse(*output,manifest,config,error,error_size) && config->time.enabled;
end:
    cJSON_Delete(edit);cJSON_Delete(doc);
    if(!ok) {free(*output);*output=NULL;return fail(error,error_size,reason);}
    return true;
}

bool epaper_photo_name_valid(const char *name)
{
    if(!name || !name[0]) return false;
    size_t n=strlen(name);
    if(n<5 || n>=256 || name[0]=='.' || name[n-1]==' ' || strstr(name,"..")) return false;
    if(name[n-4]!='.' || tolower((unsigned char)name[n-3])!='b' ||
       tolower((unsigned char)name[n-2])!='m' || tolower((unsigned char)name[n-1])!='p') return false;
    /* FAT filenames must not contain path separators, controls or reserved characters. */
    for(const unsigned char *p=(const unsigned char*)name;*p;p++) {
        if(*p<32 || *p==127 || strchr("<>:\"/\\|?*",*p)) return false;
        if(*p>=128) {
            unsigned count;uint32_t code,min;
            if(*p>=0xc2 && *p<=0xdf) {count=1;code=*p&31u;min=0x80;}
            else if(*p>=0xe0 && *p<=0xef) {count=2;code=*p&15u;min=0x800;}
            else if(*p>=0xf0 && *p<=0xf4) {count=3;code=*p&7u;min=0x10000;}
            else return false;
            while(count--) {p++;if((*p&0xc0)!=0x80) return false;code=(code<<6)|(*p&63u);}
            if(code<min || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) return false;
        }
    }
    return true;
}
bool epaper_album_file_path(const char *mount,const epaper_album_config_t *config,
    const char *album_id,const char *name,char *out,size_t size)
{
    if(!mount || !config || !album_id || !out || !size || !safe_path(config->root,true) ||
       (name && !epaper_photo_name_valid(name))) return false;
    for(unsigned i=0;i<config->album_count;i++) {
        const epaper_album_definition_t *a=&config->albums[i];
        if(strcmp(a->id,album_id)) continue;
        if(!safe_path(a->directory,false)) return false;
        int n=snprintf(out,size,"%s%s%s%s%s%s",mount,config->root,!strcmp(config->root,"/")?"":"/",
            a->directory,name?"/":"",name?name:"");
        return n>0 && (size_t)n<size;
    }
    return false;
}

bool epaper_json_read(const char *path,char **out)
{
    if(!path || !out) return false;
    *out=NULL;FILE *f=fopen(path,"rb");
    if(!f) return false;
    bool ok=false;char *text=NULL;
    if(fseek(f,0,SEEK_END)) goto finish;
    long length=ftell(f);
    if(length<=0 || length>16384 || fseek(f,0,SEEK_SET)) goto finish;
    text=malloc((size_t)length+1);
    if(!text || fread(text,1,(size_t)length,f)!=(size_t)length || ferror(f)) goto finish;
    text[length]=0;
    if(memchr(text,0,(size_t)length)) goto finish;
    *out=text;text=NULL;ok=true;
finish:
    free(text);fclose(f);return ok;
}

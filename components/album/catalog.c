#include "epaper_album.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

bool epaper_photo_path(const char *mount,const epaper_catalog_t *c,unsigned album,unsigned photo,char *out,size_t size)
{
    if(!mount || !c || !out || album>=c->config.album_count || photo>=c->albums[album].count) return false;
    int n=snprintf(out,size,"%s%s%s%s/%s",mount,c->config.root,
        !strcmp(c->config.root,"/")?"":"/",c->albums[album].definition.directory,
        c->albums[album].photos[photo].name);
    /* Root already supplies its leading slash; avoid double separators. */
    return n>0 && (size_t)n<size;
}
static int compare(const void *a,const void *b)
{ return epaper_natural_compare(((const epaper_photo_t*)a)->name,((const epaper_photo_t*)b)->name); }
static bool add_file(const char *mount,epaper_catalog_t *c,unsigned index,const char *name)
{
    size_t length=strlen(name);
    if(length<5 || length>=256 || name[length-4]!='.' ||
       tolower((unsigned char)name[length-3])!='b' || tolower((unsigned char)name[length-2])!='m' ||
       tolower((unsigned char)name[length-1])!='p') return true;
    char path[EPAPER_PATH_MAX];
    epaper_index_t *album=&c->albums[index];
    int n=snprintf(path,sizeof(path),"%s%s%s%s/%s",mount,c->config.root,
        !strcmp(c->config.root,"/")?"":"/",album->definition.directory,name);
    if(n<0 || (size_t)n>=sizeof(path)) return true;
    FILE *f=fopen(path,"rb");
    if(!f) return true;
    epaper_bmp_info_t info;
    bool valid=epaper_bmp_inspect(f,c->config.rotation,&info);fclose(f);
    if(!valid) return true;
    if(c->total>=EPAPER_MAX_PHOTOS) return false;
    epaper_photo_t *next=realloc(album->photos,(album->count+1)*sizeof(*next));
    if(!next) return false;
    album->photos=next;next[album->count]=(epaper_photo_t){0};
    strcpy(next[album->count].name,name);album->count++;c->total++;
    return true;
}
void epaper_catalog_free(epaper_catalog_t *c)
{
    if(!c) return;
    for(unsigned i=0;i<EPAPER_MAX_ALBUMS;i++) {free(c->albums[i].photos);free(c->albums[i].deck);}
    memset(c,0,sizeof(*c));
}
bool epaper_catalog_scan(const char *mount,const epaper_album_config_t *config,
                        epaper_catalog_t *out,char *error,size_t error_size)
{
    if(!mount || !config || !out) return false;
    memset(out,0,sizeof(*out));out->config=*config;
    uint32_t hash=epaper_hash(config->root,strlen(config->root),2166136261u);
    hash=epaper_hash(&config->rotation,sizeof(config->rotation),hash);
    hash=epaper_hash(&config->strategy,sizeof(config->strategy),hash);
    hash=epaper_hash(&config->shuffle,sizeof(config->shuffle),hash);
    for(unsigned i=0;i<config->album_count;i++) {
        epaper_index_t *a=&out->albums[i];a->definition=config->albums[i];
        hash=epaper_hash(a->definition.id,strlen(a->definition.id)+1,hash);
        hash=epaper_hash(a->definition.directory,strlen(a->definition.directory)+1,hash);
        hash=epaper_hash(&a->definition.interval,sizeof(a->definition.interval),hash);
        hash=epaper_hash(&a->definition.enabled,sizeof(a->definition.enabled),hash);
        if(!a->definition.enabled) continue;
        char directory[EPAPER_PATH_MAX];
        int n=snprintf(directory,sizeof(directory),"%s%s%s%s",mount,config->root,
            !strcmp(config->root,"/")?"":"/",a->definition.directory);
        if(n<0 || (size_t)n>=sizeof(directory)) goto failed;
#ifdef _WIN32
        char pattern[EPAPER_PATH_MAX+3];snprintf(pattern,sizeof(pattern),"%s/*",directory);
        struct _finddata_t entry;
        intptr_t handle=_findfirst(pattern,&entry);
        if(handle!=-1) {
            bool ok=true;
            do {
                if(!(entry.attrib&_A_SUBDIR) && !add_file(mount,out,i,entry.name)) {ok=false;break;}
            } while(_findnext(handle,&entry)==0);
            _findclose(handle);if(!ok) goto failed;
        }
#else
        DIR *dir=opendir(directory);
        if(dir) {
            bool ok=true;struct dirent *entry;
            while((entry=readdir(dir))) {
                if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
                char path[EPAPER_PATH_MAX];
                n=snprintf(path,sizeof(path),"%s/%s",directory,entry->d_name);
                struct stat statbuf;
                if(n<0 || (size_t)n>=sizeof(path) || stat(path,&statbuf) || !S_ISREG(statbuf.st_mode)) continue;
                if(!add_file(mount,out,i,entry->d_name)) {ok=false;break;}
            }
            closedir(dir);if(!ok) goto failed;
        }
#endif
        if(a->count) {
            qsort(a->photos,a->count,sizeof(*a->photos),compare);
            a->deck=malloc(a->count*sizeof(*a->deck));if(!a->deck) goto failed;
            for(size_t p=0;p<a->count;p++) hash=epaper_hash(a->photos[p].name,strlen(a->photos[p].name)+1,hash);
        }
    }
    out->fingerprint=hash;return true;
failed:
    if(error && error_size) snprintf(error,error_size,"Catalog exceeds %d images, path limit or memory",EPAPER_MAX_PHOTOS);
    epaper_catalog_free(out);return false;
}

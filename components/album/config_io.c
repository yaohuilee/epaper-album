#include "epaper_album.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define access _access
#define fileno _fileno
#define sync_file _commit
#else
#include <unistd.h>
#define sync_file fsync
#endif
static bool names(const char *path,char *backup,char *temporary,size_t size)
{
    int a=snprintf(backup,size,"%s.web-bak",path),b=snprintf(temporary,size,"%s.web-tmp",path);
    return a>0 && b>0 && (size_t)a<size && (size_t)b<size;
}
bool epaper_json_recover(const char *path)
{
    char backup[EPAPER_PATH_MAX+32],temporary[EPAPER_PATH_MAX+32];
    if(!names(path,backup,temporary,sizeof(backup))) return false;
    if(access(path,0) && !access(backup,0)) return rename(backup,path)==0;
    return true;
}
bool epaper_json_replace(const char *path,const char *text)
{
    char backup[EPAPER_PATH_MAX+32],temporary[EPAPER_PATH_MAX+32];
    if(!text || strlen(text)>16384 || !names(path,backup,temporary,sizeof(backup)) ||
       !epaper_json_recover(path) || access(path,0)) return false;
    FILE *f=fopen(temporary,"wb");if(!f) return false;
    size_t length=strlen(text);
    bool ok=fwrite(text,1,length,f)==length;
    if(fflush(f) || sync_file(fileno(f))) ok=false;
    if(fclose(f)) ok=false;
    char *readback=NULL;
    if(ok) ok=epaper_json_read(temporary,&readback) && !strcmp(text,readback);
    free(readback);
    if(!ok) {remove(temporary);return false;}
    if(!access(backup,0) && remove(backup)) return false;
    if(rename(path,backup)) return false;
    if(rename(temporary,path)) {rename(backup,path);return false;}
    return true;
}

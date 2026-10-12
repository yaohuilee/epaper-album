#include "epaper_album.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif
static int compare_files(const void *a,const void *b)
{return epaper_natural_compare(((const epaper_file_entry_t*)a)->name,((const epaper_file_entry_t*)b)->name);}
static epaper_file_result_t add(const char *mount,const epaper_album_config_t *config,const char *id,
    const char *name,epaper_file_entry_t **out,size_t *count)
{
    if(!epaper_photo_name_valid(name)) return FILE_LIST_OK;
    char path[EPAPER_PATH_MAX];struct stat st;
    if(!epaper_album_file_path(mount,config,id,name,path,sizeof(path))) return FILE_LIST_INVALID;
    if(stat(path,&st)) return FILE_LIST_IO;
    if((st.st_mode&S_IFMT)!=S_IFREG) return FILE_LIST_OK;
    if(*count>=EPAPER_MAX_PHOTOS) return FILE_LIST_LIMIT;
    epaper_file_entry_t *next=realloc(*out,(*count+1)*sizeof(**out));
    if(!next) return FILE_LIST_MEMORY;
    *out=next;epaper_file_entry_t *entry=&next[(*count)++];memset(entry,0,sizeof(*entry));
    strcpy(entry->name,name);entry->bytes=(uint64_t)st.st_size;
    FILE *f=fopen(path,"rb");if(!f) return FILE_LIST_IO;
    epaper_bmp_info_t info;entry->header_valid=epaper_bmp_inspect(f,config->rotation,&info);fclose(f);
    return FILE_LIST_OK;
}
epaper_file_result_t epaper_files_list(const char *mount,const epaper_album_config_t *config,
    const char *id,epaper_file_entry_t **out,size_t *count)
{
    if(!out || !count) return FILE_LIST_INVALID;
    *out=NULL;*count=0;char directory[EPAPER_PATH_MAX];struct stat st;
    if(!epaper_album_file_path(mount,config,id,NULL,directory,sizeof(directory))) return FILE_LIST_INVALID;
    if(stat(directory,&st) || (st.st_mode&S_IFMT)!=S_IFDIR) return FILE_LIST_NOT_FOUND;
    epaper_file_result_t result=FILE_LIST_OK;
#ifdef _WIN32
    char pattern[EPAPER_PATH_MAX+3];snprintf(pattern,sizeof(pattern),"%s/*",directory);
    struct _finddata_t entry;intptr_t handle=_findfirst(pattern,&entry);
    if(handle!=-1) {
        do {if(!(entry.attrib&_A_SUBDIR)) result=add(mount,config,id,entry.name,out,count);}
        while(result==FILE_LIST_OK && _findnext(handle,&entry)==0);
        _findclose(handle);
    }
#else
    DIR *dir=opendir(directory);if(!dir) return FILE_LIST_IO;
    struct dirent *entry;
    while((entry=readdir(dir))) {
        result=add(mount,config,id,entry->d_name,out,count);
        if(result!=FILE_LIST_OK) break;
    }
    closedir(dir);
#endif
    if(result!=FILE_LIST_OK) {free(*out);*out=NULL;*count=0;}
    else if(*count) qsort(*out,*count,sizeof(**out),compare_files);
    return result;
}
bool epaper_photo_validate(const char *path,int rotation)
{
    FILE *f=fopen(path,"rb");if(!f) return false;
    /* Read multiple pixel rows per FAT operation. A failed allocation keeps
       the default buffer and validation still checks every pixel. */
    (void)setvbuf(f,NULL,_IOFBF,32768);
    uint8_t *frame=malloc(EPAPER_FRAME_BYTES);uint32_t hash;
    bool ok=frame && epaper_bmp_decode(f,rotation,frame,&hash);
    free(frame);fclose(f);return ok;
}

#include "epaper_bmp.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }

uint32_t epaper_hash(const void *data, size_t length, uint32_t initial)
{
    const uint8_t *p = data;
    uint32_t hash = initial;
    for (size_t i=0; i<length; i++) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}

bool epaper_bmp_inspect(FILE *file, int rotation, epaper_bmp_info_t *out)
{
    if (!file || !out || (rotation!=0 && rotation!=90 && rotation!=180 && rotation!=270)) return false;
    uint8_t h[54];
    if (fseek(file,0,SEEK_SET) || fread(h,1,sizeof(h),file)!=sizeof(h)) return false;
    if (h[0]!='B' || h[1]!='M' || le32(h+14)!=40 || le16(h+26)!=1 ||
        le16(h+28)!=24 || le32(h+30)!=0) return false;
    uint32_t w=le32(h+18), height=le32(h+22), offset=le32(h+10);
    bool portrait = rotation==90 || rotation==270;
    if (w!=(portrait?480u:800u) || height!=(portrait?800u:480u) || offset<54) return false;
    uint32_t stride=(w*3+3)&~3u;
    if (offset>0x7fffffffu-stride*height || fseek(file,0,SEEK_END)) return false;
    long size=ftell(file);
    if (size<0 || (uint32_t)size<offset+stride*height || le32(h+2)!=(uint32_t)size) return false;
    uint32_t pixels_size=le32(h+34);
    if (pixels_size!=0 && pixels_size!=stride*height) return false;
    *out=(epaper_bmp_info_t){w,height,offset,stride};
    return true;
}

static int color(const uint8_t *p)
{
    uint32_t rgb=(uint32_t)p[2]<<16 | (uint32_t)p[1]<<8 | p[0];
    switch(rgb) {
    case 0x000000: return 0; case 0xffffff: return 1; case 0xffff00: return 2;
    case 0xff0000: return 3; case 0x0000ff: return 5; case 0x00ff00: return 6;
    default: return -1;
    }
}

bool epaper_bmp_decode(FILE *file, int rotation, uint8_t frame[EPAPER_FRAME_BYTES], uint32_t *hash)
{
    epaper_bmp_info_t info;
    if (!frame || !hash || !epaper_bmp_inspect(file,rotation,&info)) return false;
    uint8_t *row=malloc(info.stride);
    if (!row) return false;
    bool ok=fseek(file,(long)info.offset,SEEK_SET)==0;
    memset(frame,0x11,EPAPER_FRAME_BYTES);
    for (uint32_t file_y=0; ok && file_y<info.height; file_y++) {
        if (fread(row,1,info.stride,file)!=info.stride) { ok=false; break; }
        unsigned y=info.height-1-file_y;
        for (unsigned x=0; x<info.width; x++) {
            int code=color(row+3*x);
            if (code<0) { ok=false; break; }
            unsigned dx=x,dy=y;
            if(rotation==90) { dx=799-y; dy=x; }
            else if(rotation==180) { dx=799-x; dy=479-y; }
            else if(rotation==270) { dx=y; dy=479-x; }
            size_t index=(size_t)dy*400+dx/2;
            if(dx&1) frame[index]=(uint8_t)((frame[index]&0xf0)|code);
            else frame[index]=(uint8_t)((frame[index]&0x0f)|(code<<4));
        }
    }
    free(row);
    if(ok) *hash=epaper_hash(frame,EPAPER_FRAME_BYTES,2166136261u);
    return ok;
}

int epaper_natural_compare(const char *a, const char *b)
{
    const char *original_a=a,*original_b=b;
    while(*a && *b) {
        if(isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            const char *ea=a,*eb=b;
            while(isdigit((unsigned char)*ea)) ea++;
            while(isdigit((unsigned char)*eb)) eb++;
            while(a<ea && *a=='0') a++;
            while(b<eb && *b=='0') b++;
            size_t na=(size_t)(ea-a),nb=(size_t)(eb-b);
            if(na!=nb) return na<nb?-1:1;
            int cmp=memcmp(a,b,na);
            if(cmp) return cmp<0?-1:1;
            a=ea;b=eb;
        } else {
            int ca=tolower((unsigned char)*a),cb=tolower((unsigned char)*b);
            if(ca!=cb) return ca<cb?-1:1;
            a++;b++;
        }
    }
    if(*a || *b) return *a?1:-1;
    return strcmp(original_a,original_b);
}

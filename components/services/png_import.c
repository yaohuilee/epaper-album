#include "png_import.h"
#include "png.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

typedef union {max_align_t alignment;size_t bytes;} allocation_t;
typedef struct {
    FILE *file;
    const epaper_png_hooks_t *hooks;
    png_structp png;
    png_infop info;
    uint8_t *row,*accum;
    size_t allocated,peak;
    unsigned source_width,source_height;
    bool ok;
    char error[128];
} png_context_t;

static png_voidp allocate(png_structp png,png_alloc_size_t bytes)
{
    png_context_t *ctx=png_get_mem_ptr(png);
    if(bytes>262144 || ctx->allocated+bytes>768u*1024u)return NULL;
    allocation_t *p=heap_caps_malloc(sizeof(*p)+(size_t)bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!p)return NULL;
    p->bytes=(size_t)bytes;ctx->allocated+=(size_t)bytes;
    if(ctx->allocated>ctx->peak)ctx->peak=ctx->allocated;
    return p+1;
}
static void release(png_structp png,png_voidp memory)
{
    if(!memory)return;
    png_context_t *ctx=png_get_mem_ptr(png);allocation_t *p=(allocation_t*)memory-1;
    ctx->allocated-=p->bytes;free(p);
}
static void failed(png_structp png,png_const_charp message)
{
    png_context_t *ctx=png_get_error_ptr(png);
    snprintf(ctx->error,sizeof(ctx->error),"PNG decoding failed: %s",message);
    png_longjmp(png,1);
}
static void warning(png_structp png,png_const_charp message){(void)png;(void)message;}
static void read_bytes(png_structp png,png_bytep buffer,png_size_t bytes)
{
    png_context_t *ctx=png_get_io_ptr(png);
    while(bytes) {
        size_t count=bytes>8192?8192:bytes;
        ctx->hooks->lock();size_t got=fread(buffer,1,count,ctx->file);ctx->hooks->unlock();
        if(got!=count)png_error(png,"truncated source or SD read failure");
        buffer+=count;bytes-=count;
    }
}
static int unknown_chunk(png_structp png,png_unknown_chunkp chunk)
{
    if(!memcmp(chunk->name,"acTL",4) || !memcmp(chunk->name,"fcTL",4) || !memcmp(chunk->name,"fdAT",4))
        png_error(png,"animated PNG is not supported");
    if(!(chunk->name[0]&0x20))png_error(png,"unknown critical chunk");
    return 1;
}
bool epaper_png_decode(FILE *file,epaper_image_map_t *map,unsigned width,unsigned height,
    uint8_t **rgb,const epaper_png_hooks_t *hooks,char *error,size_t error_size)
{
    *rgb=NULL;png_context_t *ctx=calloc(1,sizeof(*ctx));
    if(!ctx){snprintf(error,error_size,"Insufficient PNG task memory");return false;}
    ctx->file=file;ctx->hooks=hooks;
    ctx->png=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,ctx,failed,warning,ctx,allocate,release);
    if(!ctx->png){snprintf(error,error_size,"Insufficient PNG decoder memory");free(ctx);return false;}
    /* Everything modified after setjmp is heap-owned, including failure cleanup. */
    if(setjmp(png_jmpbuf(ctx->png)))goto cleanup;
    ctx->info=png_create_info_struct(ctx->png);
    if(!ctx->info)png_error(ctx->png,"out of memory");
    png_set_read_fn(ctx->png,ctx,read_bytes);
    png_set_user_limits(ctx->png,EPAPER_IMAGE_MAX_EDGE,EPAPER_IMAGE_MAX_EDGE);
    png_set_chunk_malloc_max(ctx->png,65536);png_set_chunk_cache_max(ctx->png,8);
    png_set_crc_action(ctx->png,PNG_CRC_ERROR_QUIT,PNG_CRC_ERROR_QUIT);
    /* Ignore metadata, particularly compressed text and ICC profiles. Keep APNG
       control chunks only long enough to reject animation explicitly. */
    static const png_byte ignored[]="cHRM\0gAMA\0iCCP\0sRGB\0bKGD\0hIST\0pHYs\0sBIT\0sCAL\0pCAL\0tIME\0eXIf\0sPLT\0tEXt\0zTXt\0iTXt\0";
    static const png_byte animation[]="acTL\0fcTL\0fdAT\0";
    png_set_keep_unknown_chunks(ctx->png,PNG_HANDLE_CHUNK_NEVER,NULL,0);
    png_set_keep_unknown_chunks(ctx->png,PNG_HANDLE_CHUNK_NEVER,ignored,16);
    png_set_keep_unknown_chunks(ctx->png,PNG_HANDLE_CHUNK_ALWAYS,animation,3);
    png_set_read_user_chunk_fn(ctx->png,ctx,unknown_chunk);
    png_read_info(ctx->png,ctx->info);
    ctx->source_width=png_get_image_width(ctx->png,ctx->info);ctx->source_height=png_get_image_height(ctx->png,ctx->info);
    if((uint64_t)ctx->source_width*ctx->source_height>EPAPER_IMAGE_MAX_PIXELS ||
       !epaper_image_map(map,ctx->source_width,ctx->source_height,width,height,1))
        png_error(ctx->png,"image exceeds dimension or pixel limits");
    int color=png_get_color_type(ctx->png,ctx->info),depth=png_get_bit_depth(ctx->png,ctx->info);
    bool transparent=png_get_valid(ctx->png,ctx->info,PNG_INFO_tRNS)!=0;
    if(depth==16)png_set_strip_16(ctx->png);
    if(color==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(ctx->png);
    if(color==PNG_COLOR_TYPE_GRAY && depth<8)png_set_expand_gray_1_2_4_to_8(ctx->png);
    if(transparent)png_set_tRNS_to_alpha(ctx->png);
    if(color==PNG_COLOR_TYPE_GRAY || color==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(ctx->png);
    if(!(color&PNG_COLOR_MASK_ALPHA) && !transparent)png_set_add_alpha(ctx->png,255,PNG_FILLER_AFTER);
    int passes=png_set_interlace_handling(ctx->png);png_read_update_info(ctx->png,ctx->info);
    size_t rowbytes=png_get_rowbytes(ctx->png,ctx->info),pixels=(size_t)width*height;
    if(rowbytes!=(size_t)ctx->source_width*4 || png_get_bit_depth(ctx->png,ctx->info)!=8)
        png_error(ctx->png,"unsupported row format");
    ctx->row=heap_caps_malloc(rowbytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    ctx->accum=heap_caps_calloc(pixels*3,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!ctx->row || !ctx->accum)png_error(ctx->png,"Insufficient PSRAM for resized PNG");
    for(int pass=0;pass<passes;pass++)for(unsigned y=0;y<ctx->source_height;y++) {
        hooks->lock();hooks->unlock();
        png_read_row(ctx->png,ctx->row,NULL);
        epaper_image_add_rgba_row(map,(uint16_t*)ctx->accum,ctx->row,y,passes==1?7:(unsigned)pass);
        if(!(y%8)) {
            hooks->report(ctx->source_width,ctx->source_height,5+25u*((unsigned)pass*ctx->source_height+y+1)/((unsigned)passes*ctx->source_height),
                pixels*6+rowbytes+sizeof(*map)+ctx->peak+((size_t)width+2)*24+width*3+32768);
            vTaskDelay(1);
        }
    }
    png_read_end(ctx->png,ctx->info);
    for(size_t i=0;i<pixels*3;i++)ctx->accum[i]=(uint8_t)(((uint16_t*)ctx->accum)[i]>>8);
    *rgb=ctx->accum;ctx->accum=NULL;ctx->ok=true;
cleanup:
    if(!ctx->ok)snprintf(error,error_size,"%s",ctx->error[0]?ctx->error:"PNG decoding failed");
    free(ctx->row);free(ctx->accum);png_destroy_read_struct(&ctx->png,&ctx->info,NULL);
    bool ok=ctx->ok;free(ctx);return ok;
}

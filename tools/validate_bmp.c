/* Native CLI validator using the same decoder as the device. */
#include "epaper_bmp.h"
#include <stdlib.h>
#include <inttypes.h>
int main(int argc,char **argv)
{
    if(argc<2) {fprintf(stderr,"Usage: validate_bmp file.bmp [...] (rotation 0)\n");return 2;}
    uint8_t *frame=malloc(EPAPER_FRAME_BYTES);
    if(!frame) return 2;
    int result=0;
    for(int i=1;i<argc;i++) {
        FILE *file=fopen(argv[i],"rb");uint32_t hash=0;
        bool valid=file && epaper_bmp_decode(file,0,frame,&hash);
        if(file) fclose(file);
        printf("%s %s frame_hash=%08"PRIx32"\n",valid?"VALID":"INVALID",argv[i],hash);
        if(!valid) result=1;
    }
    free(frame);return result;
}

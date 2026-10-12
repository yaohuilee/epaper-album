#include "provision_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"Provision test failed: %s\n",#x);return 1;}} while(0)
int main(void)
{
    uint8_t *bytes=malloc(EPAPER_FRAME_BYTES+2);CHECK(bytes);
    bytes[0]=bytes[EPAPER_FRAME_BYTES+1]=0xa5;
    CHECK(epaper_provision_render(bytes+1,"epaper-album-ABC123"));
    CHECK(bytes[0]==0xa5 && bytes[EPAPER_FRAME_BYTES+1]==0xa5);
    for(size_t i=1;i<=EPAPER_FRAME_BYTES;i++) {
        unsigned lo=bytes[i]&15,hi=bytes[i]>>4;
        CHECK((lo<=1 || lo==5) && (hi<=1 || hi==5));
    }
    FILE *f=fopen(".local/provision-frame.bin","wb");CHECK(f);
    CHECK(fwrite(bytes+1,1,EPAPER_FRAME_BYTES,f)==EPAPER_FRAME_BYTES);CHECK(!fclose(f));
    CHECK(!epaper_provision_render(NULL,"valid"));
    CHECK(!epaper_provision_render(bytes+1,"ssid-longer-than-thirty-two-bytes-rejected"));
    free(bytes);puts("provision screen tests: bounds, black/white/blue palette and invalid input passed");return 0;
}

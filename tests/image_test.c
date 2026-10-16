#include "epaper_image.h"
#include "epaper_bmp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Image FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
int main(void)
{
    epaper_image_map_t *m=malloc(sizeof(*m));CHECK(m);
    CHECK(epaper_image_map(m,1600,900,800,480,1));CHECK(m->x[0].low==50 && m->x[799].high==1549);
    for(unsigned orientation=1;orientation<=8;orientation++) {
        CHECK(epaper_image_map(m,7,5,8,8,orientation));
        for(unsigned y=0;y<5;y++)for(unsigned x=0;x<7;x++){unsigned ox,oy,sx,sy;epaper_image_oriented(m,x,y,&ox,&oy);epaper_image_original(m,ox,oy,&sx,&sy);CHECK(x==sx&&y==sy);}
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){unsigned w[4];epaper_image_weights(m,x,y,w);CHECK(w[0]+w[1]+w[2]+w[3]==256);CHECK(epaper_image_first(m->x,8,m->x[x].high)<=x);}
    }
    /* Bilinear accumulation must be independent of MCU boundaries and orientation. */
    uint8_t source_rgb[9*5*3];for(unsigned y=0;y<5;y++)for(unsigned x=0;x<9;x++){source_rgb[(y*9+x)*3]=(uint8_t)(x*17);source_rgb[(y*9+x)*3+1]=(uint8_t)(y*43);source_rgb[(y*9+x)*3+2]=(uint8_t)((x+y)*13);}
    for(unsigned o=1;o<=8;o++) {
        CHECK(epaper_image_map(m,9,5,11,7,o));uint16_t accum[11*7*3]={0};
        for(unsigned by=0;by<5;by+=2)for(unsigned bx=0;bx<9;bx+=3){unsigned right=bx+2,bottom=by+1;if(bottom>4)bottom=4;uint8_t block[3*2*3];for(unsigned y=by;y<=bottom;y++)memcpy(block+(y-by)*9,source_rgb+(y*9+bx)*3,9);epaper_image_add_block(m,accum,block,bx,right,by,bottom);}
        for(unsigned y=0;y<7;y++)for(unsigned x=0;x<11;x++){unsigned weights[4];epaper_image_weights(m,x,y,weights);for(unsigned c=0;c<3;c++){unsigned expected=0;for(unsigned k=0;k<4;k++){unsigned sx,sy;epaper_image_original(m,(k&1)?m->x[x].high:m->x[x].low,(k&2)?m->y[y].high:m->y[y].low,&sx,&sy);expected+=source_rgb[(sy*9+sx)*3+c]*weights[k];}CHECK(accum[(y*11+x)*3+c]==expected);}}
    }
    /* RGBA compositing and Adam7 contributions must produce the same samples. */
    for(unsigned sw=1;sw<=17;sw++)for(unsigned sh=1;sh<=11;sh++) {
        uint8_t rgba[17*11*4];uint16_t full[13*9*3]={0},adam[13*9*3]={0};
        for(unsigned i=0;i<sw*sh;i++){rgba[i*4]=(uint8_t)(i*13);rgba[i*4+1]=(uint8_t)(i*29);rgba[i*4+2]=(uint8_t)(i*47);rgba[i*4+3]=(uint8_t)(i%3==0?0:i%3==1?128:255);}
        CHECK(epaper_image_map(m,sw,sh,13,9,1));
        for(unsigned y=0;y<sh;y++)epaper_image_add_rgba_row(m,full,rgba+y*sw*4,y,7);
        for(unsigned pass=0;pass<7;pass++)for(unsigned y=0;y<sh;y++)epaper_image_add_rgba_row(m,adam,rgba+y*sw*4,y,pass);
        for(unsigned y=0;y<9;y++)for(unsigned x=0;x<13;x++) {
            unsigned weights[4];epaper_image_weights(m,x,y,weights);
            for(unsigned c=0;c<3;c++) {
                unsigned expected=0;
                for(unsigned k=0;k<4;k++){const uint8_t *p=rgba+(((k&2)?m->y[y].high:m->y[y].low)*sw+((k&1)?m->x[x].high:m->x[x].low))*4;expected+=((p[c]*p[3]+255u*(255u-p[3])+127u)/255u)*weights[k];}
                CHECK(full[(y*13+x)*3+c]==expected);CHECK(full[(y*13+x)*3+c]==adam[(y*13+x)*3+c]);
            }
        }
    }
    CHECK(!epaper_image_map(m,0,1,800,480,1));CHECK(!epaper_image_map(m,12001,1,800,480,1));
    uint8_t header[54];epaper_image_bmp_t bmp;epaper_image_bmp_output_header(header,800,480);
    CHECK(epaper_image_bmp_header(header,1152054,&bmp));CHECK(bmp.width==800&&bmp.height==480&&!bmp.top_down);
    put32(header+22,(uint32_t)(int32_t)-480);CHECK(epaper_image_bmp_header(header,1152054,&bmp)&&bmp.top_down);
    header[28]=16;CHECK(!epaper_image_bmp_header(header,1152054,&bmp));header[28]=24;
    header[30]=1;CHECK(!epaper_image_bmp_header(header,1152054,&bmp));header[30]=0;
    CHECK(!epaper_image_bmp_header(header,1152053,&bmp));put32(header+22,0x80000000);CHECK(!epaper_image_bmp_header(header,1152054,&bmp));
    uint8_t exif[32]={'E','x','i','f',0,0,'I','I',42,0,8,0,0,0,1,0,0x12,1,3,0,1,0,0,0,6,0,0,0,0,0,0,0};
    CHECK(epaper_image_exif_orientation(exif,sizeof(exif))==6);CHECK(epaper_image_exif_orientation(exif,15)==1);exif[24]=9;CHECK(epaper_image_exif_orientation(exif,32)==1);exif[10]=255;CHECK(epaper_image_exif_orientation(exif,32)==1);
    const unsigned width=800,height=480;uint8_t *rgb=malloc(width*height*3),*bgr=malloc(width*3);int32_t *errors=calloc((width+2)*6,sizeof(*errors));CHECK(rgb&&bgr&&errors);
    memset(rgb,128,width*height*3);uint8_t lut[256];for(unsigned i=0;i<256;i++)lut[i]=(uint8_t)i;
    int32_t *current=errors,*next=errors+(width+2)*3;uint64_t sums[3]={0};
    for(unsigned y=0;y<height;y++) {
        epaper_image_dither_row(rgb+y*width*3,width,y,lut,current,next,bgr);
        for(unsigned x=0;x<width;x++){uint8_t *p=bgr+x*3;bool supported=(!p[0]&&!p[1]&&!p[2])||(p[0]==255&&p[1]==255&&p[2]==255)||(!p[0]&&p[1]==255&&p[2]==255)||(!p[0]&&!p[1]&&p[2]==255)||(p[0]==255&&!p[1]&&!p[2])||(!p[0]&&p[1]==255&&!p[2]);CHECK(supported);for(unsigned c=0;c<3;c++)sums[c]+=p[c];}
        int32_t *swap=current;current=next;next=swap;
    }
    for(unsigned c=0;c<3;c++){double mean=(double)sums[c]/(width*height);CHECK(mean>118 && mean<138);}
    epaper_image_tone(rgb,width*height,lut);CHECK(lut[0]==0 && lut[255]==255);
    free(rgb);free(bgr);free(errors);free(m);printf("image tests: %u checks passed\n",checks);return 0;
}

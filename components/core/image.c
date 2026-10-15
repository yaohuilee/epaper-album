#include "epaper_image.h"
#include <string.h>
static uint16_t le16(const uint8_t *p){return (uint16_t)(p[0]|p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
bool epaper_image_bmp_header(const uint8_t h[54],uint64_t bytes,epaper_image_bmp_t *out)
{
    if(!h || !out || h[0]!='B' || h[1]!='M' || le32(h+14)<40 || le32(h+14)>124 || le16(h+26)!=1 || le16(h+28)!=24 || le32(h+30)!=0)return false;
    int32_t w=(int32_t)le32(h+18),signed_h=(int32_t)le32(h+22);
    if(w<=0 || !signed_h || signed_h==INT32_MIN)return false;
    unsigned height=(unsigned)(signed_h<0?-signed_h:signed_h),width=(unsigned)w;
    uint32_t offset=le32(h+10),stride=(width*3+3)&~3u;
    if(width>EPAPER_IMAGE_MAX_EDGE || height>EPAPER_IMAGE_MAX_EDGE || (uint64_t)width*height>EPAPER_IMAGE_MAX_PIXELS ||
       offset<14+le32(h+14) || (uint64_t)offset+(uint64_t)stride*height>bytes || le32(h+2)!=bytes)return false;
    *out=(epaper_image_bmp_t){width,height,stride,offset,signed_h<0};return true;
}
void epaper_image_bmp_output_header(uint8_t h[54],unsigned w,unsigned height)
{
    memset(h,0,54);h[0]='B';h[1]='M';put32(h+2,54+w*height*3);put32(h+10,54);put32(h+14,40);put32(h+18,w);put32(h+22,height);h[26]=1;h[28]=24;put32(h+34,w*height*3);
}
static void samples(epaper_image_sample_t *out,unsigned count,unsigned source,uint64_t extent,uint64_t origin)
{
    for(unsigned i=0;i<count;i++) {
        int64_t point=(int64_t)(origin+((uint64_t)(2*i+1)*extent)/(2*count))-32768;
        if(point<0)point=0;
        if((uint64_t)point>((uint64_t)source-1)*65536)point=(int64_t)(source-1)*65536;
        unsigned low=(unsigned)(point/65536);out[i]=(epaper_image_sample_t){(uint16_t)low,(uint16_t)(low+1<source?low+1:low),(uint8_t)((point&65535)>>8)};
    }
}
bool epaper_image_map(epaper_image_map_t *m,unsigned sw,unsigned sh,unsigned w,unsigned h,unsigned o)
{
    if(!m || !sw || !sh || !w || !h || sw>EPAPER_IMAGE_MAX_EDGE || sh>EPAPER_IMAGE_MAX_EDGE || w>800 || h>800 || o<1 || o>8)return false;
    m->source_width=sw;m->source_height=sh;m->width=w;m->height=h;m->orientation=o;
    unsigned ow=o>=5?sh:sw,oh=o>=5?sw:sh;
    uint64_t cw=(uint64_t)ow*65536,ch=(uint64_t)oh*65536;
    if((uint64_t)ow*h>(uint64_t)oh*w)cw=ch*w/h;else ch=cw*h/w;
    samples(m->x,w,ow,cw,((uint64_t)ow*65536-cw)/2);samples(m->y,h,oh,ch,((uint64_t)oh*65536-ch)/2);return true;
}
void epaper_image_original(const epaper_image_map_t *m,unsigned x,unsigned y,unsigned *sx,unsigned *sy)
{
    unsigned w=m->source_width,h=m->source_height;
    switch(m->orientation){
    case 2:*sx=w-1-x;*sy=y;break;case 3:*sx=w-1-x;*sy=h-1-y;break;case 4:*sx=x;*sy=h-1-y;break;
    case 5:*sx=y;*sy=x;break;case 6:*sx=y;*sy=h-1-x;break;case 7:*sx=w-1-y;*sy=h-1-x;break;case 8:*sx=w-1-y;*sy=x;break;
    default:*sx=x;*sy=y;break;}
}
void epaper_image_oriented(const epaper_image_map_t *m,unsigned sx,unsigned sy,unsigned *x,unsigned *y)
{
    unsigned w=m->source_width,h=m->source_height;
    switch(m->orientation){
    case 2:*x=w-1-sx;*y=sy;break;case 3:*x=w-1-sx;*y=h-1-sy;break;case 4:*x=sx;*y=h-1-sy;break;
    case 5:*x=sy;*y=sx;break;case 6:*x=h-1-sy;*y=sx;break;case 7:*x=h-1-sy;*y=w-1-sx;break;case 8:*x=sy;*y=w-1-sx;break;
    default:*x=sx;*y=sy;break;}
}
void epaper_image_weights(const epaper_image_map_t *m,unsigned x,unsigned y,unsigned w[4])
{
    unsigned fx=m->x[x].fraction,fy=m->y[y].fraction;
    w[0]=((256-fx)*(256-fy))>>8;w[1]=(fx*(256-fy))>>8;w[2]=((256-fx)*fy)>>8;w[3]=256-w[0]-w[1]-w[2];
}
unsigned epaper_image_first(const epaper_image_sample_t *m,unsigned count,unsigned edge)
{
    unsigned l=0,r=count;while(l<r){unsigned mid=(l+r)/2;if(m[mid].high<edge)l=mid+1;else r=mid;}return l;
}
static unsigned clip(int v){return (unsigned)(v<0?0:v>255?255:v);}
void epaper_image_tone(const uint8_t *rgb,size_t pixels,uint8_t lut[256])
{
    uint32_t hist[256]={0};for(size_t i=0;i<pixels;i++)hist[(54u*rgb[i*3]+183u*rgb[i*3+1]+19u*rgb[i*3+2])>>8]++;
    unsigned low=0,high=255;size_t cutoff=pixels*4/1000,total=0;
    while(low<255 && total+hist[low]<=cutoff)total+=hist[low++];
    total=0;while(high>0 && total+hist[high]<=cutoff)total+=hist[high--];
    for(unsigned i=0;i<256;i++){int v=high>low?((int)i-(int)low)*255/(int)(high-low):(int)i;lut[i]=(uint8_t)clip((v-128)*106/100+128);}
}
void epaper_image_dither_row(const uint8_t *rgb,unsigned width,unsigned y,const uint8_t lut[256],int32_t *cur,int32_t *next,uint8_t *bgr)
{
    static const uint8_t colors[6][3]={{0,0,0},{255,255,255},{255,255,0},{255,0,0},{0,0,255},{0,255,0}};
    memset(next,0,(width+2)*3*sizeof(*next));int direction=(y&1)?-1:1;
    for(unsigned step=0;step<width;step++) {
        unsigned x=direction>0?step:width-1-step;int v[3];
        for(unsigned c=0;c<3;c++)v[c]=(int)clip((int)lut[rgb[x*3+c]]+cur[(x+1)*3+c]/16);
        unsigned selected=0;uint32_t best=UINT32_MAX;
        for(unsigned k=0;k<6;k++){int dr=v[0]-colors[k][0],dg=v[1]-colors[k][1],db=v[2]-colors[k][2];uint32_t d=(uint32_t)(30*dr*dr+59*dg*dg+11*db*db);if(d<best){best=d;selected=k;}}
        for(unsigned c=0;c<3;c++){
            int e=v[c]-colors[selected][c];int p=(int)(x+1)*3+(int)c;
            cur[p+direction*3]+=e*7;next[p-direction*3]+=e*3;next[p]+=e*5;next[p+direction*3]+=e;
            bgr[x*3+2-c]=colors[selected][c];
        }
    }
}
unsigned epaper_image_exif_orientation(const uint8_t *p,size_t n)
{
    if(n<14 || memcmp(p,"Exif\0\0",6))return 1;
    p+=6;n-=6;
    bool little=p[0]=='I'&&p[1]=='I';if(!little && !(p[0]=='M'&&p[1]=='M'))return 1;
    #define U16(q) (little?le16(q):(uint16_t)((q)[0]<<8|(q)[1]))
    #define U32(q) (little?le32(q):((uint32_t)(q)[0]<<24|(uint32_t)(q)[1]<<16|(uint32_t)(q)[2]<<8|(q)[3]))
    if(U16(p+2)!=42)return 1;
    uint32_t offset=U32(p+4);if(offset>n-2)return 1;
    unsigned count=U16(p+offset);if(count>(n-offset-2)/12)return 1;
    for(unsigned i=0;i<count;i++){const uint8_t *e=p+offset+2+i*12;if(U16(e)==0x112 && U16(e+2)==3 && U32(e+4)==1){unsigned o=U16(e+8);return o>=1&&o<=8?o:1;}}
    #undef U16
    #undef U32
    return 1;
}

void epaper_image_add_block(const epaper_image_map_t *m,uint16_t *accum,const uint8_t *bitmap,unsigned left_edge,unsigned right_edge,unsigned top_edge,unsigned bottom_edge)
{
    unsigned ox[4],oy[4];if(left_edge>=m->source_width || top_edge>=m->source_height)return;
    unsigned rr=right_edge<m->source_width?right_edge:m->source_width-1,bb=bottom_edge<m->source_height?bottom_edge:m->source_height-1;
    epaper_image_oriented(m,left_edge,top_edge,&ox[0],&oy[0]);epaper_image_oriented(m,rr,top_edge,&ox[1],&oy[1]);epaper_image_oriented(m,left_edge,bb,&ox[2],&oy[2]);epaper_image_oriented(m,rr,bb,&ox[3],&oy[3]);
    unsigned left=ox[0],right=ox[0],top=oy[0],bottom=oy[0];for(unsigned i=1;i<4;i++){if(ox[i]<left)left=ox[i];if(ox[i]>right)right=ox[i];if(oy[i]<top)top=oy[i];if(oy[i]>bottom)bottom=oy[i];}
    unsigned first_x=epaper_image_first(m->x,m->width,left),first_y=epaper_image_first(m->y,m->height,top),stride=right_edge-left_edge+1;
    const uint8_t *rgb=bitmap;
    for(unsigned y=first_y;y<m->height && m->y[y].low<=bottom;y++)for(unsigned x=first_x;x<m->width && m->x[x].low<=right;x++) {
        unsigned weights[4];epaper_image_weights(m,x,y,weights);
        for(unsigned corner=0;corner<4;corner++)if(weights[corner]) {
            unsigned sx,sy;epaper_image_original(m,(corner&1)?m->x[x].high:m->x[x].low,(corner&2)?m->y[y].high:m->y[y].low,&sx,&sy);
            if(sx>=left_edge && sx<=right_edge && sy>=top_edge && sy<=bottom_edge) {
                const uint8_t *p=rgb+((sy-top_edge)*stride+sx-left_edge)*3;uint16_t *d=accum+((size_t)y*m->width+x)*3;
                for(unsigned c=0;c<3;c++)d[c]=(uint16_t)(d[c]+p[c]*weights[corner]);
            }
        }
    }
}

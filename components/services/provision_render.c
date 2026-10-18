#include "provision_address.h"
#include "provision_render.h"
#include "qrcodegen.h"
#include "provision_font.h"
#include <string.h>
#include <stdio.h>
/* Provisioning uses black 0, white 1 and blue 5 only. */
/* 8px spacing grid: 24px margins, two 360px columns, 32px gutter.
 * Each column contains a centered 232px QR area and an ink-centered caption.
 * Mixed Chinese/ASCII rows share a baseline from generated font metrics. */
enum { MARGIN=24, CENTER=400, LABEL_GAP=24, COLUMN=360, GUTTER=32, QR_AREA=232,
       LEFT_QR=MARGIN+(COLUMN-QR_AREA)/2,
       RIGHT_QR=LEFT_QR+COLUMN+GUTTER };
static void pixel(uint8_t *frame,int x,int y,unsigned color)
{
    if(x<0 || x>=800 || y<0 || y>=480) return;
    size_t i=(y*800+x)/2;
    if(x%2) frame[i]=(uint8_t)((frame[i]&0xf0)|color);
    else frame[i]=(uint8_t)((frame[i]&0x0f)|(color<<4));
}
static void rect(uint8_t *frame,int x,int y,int width,int height,unsigned color)
{
    for(int j=0;j<height;j++) for(int i=0;i<width;i++) pixel(frame,x+i,y+j,color);
}
static void bitmap(uint8_t *frame,const provision_glyph_t *glyph,int x,int y,unsigned color)
{
    for(unsigned j=0;j<glyph->height;j++) for(unsigned i=0;i<glyph->width;i++)
        if(glyph->bits[j*(glyph->width/8)+i/8]&(0x80>>(i%8))) pixel(frame,x+i,y+j,color);
}
static bool glyph_bounds(const provision_glyph_t *glyph,int *first,int *last)
{
    *first=(int)glyph->width;*last=-1;
    for(unsigned j=0;j<glyph->height;j++) for(unsigned i=0;i<glyph->width;i++)
        if(glyph->bits[j*(glyph->width/8)+i/8]&(0x80>>(i%8))) {
            if((int)i<*first) *first=(int)i;
            if((int)i>*last) *last=(int)i;
        }
    return *last>=*first;
}
static void centered_bitmap(uint8_t *frame,const provision_glyph_t *glyph,int center,int y,unsigned color)
{
    int first,last;
    if(glyph_bounds(glyph,&first,&last)) bitmap(frame,glyph,center-(first+last)/2,y,color);
}
static void text(uint8_t *frame,const char *s,int x,int baseline,unsigned color)
{
    for(;*s;s++,x+=16) if(*s>=32 && *s<=126) {
        const provision_glyph_t *glyph=ascii_glyphs[*s-32];
        bitmap(frame,glyph,x,baseline-(int)glyph->baseline,color);
    }
}
static void centered_row(uint8_t *frame,const provision_glyph_t *label,const char *value,int baseline)
{
    int label_y=baseline-(int)label->baseline;
    int label_first,label_last;
    if(!glyph_bounds(label,&label_first,&label_last)) return;
    int value_first=800,value_last=-1,offset=0;
    for(const char *s=value;*s;s++,offset+=16) if(*s>=32 && *s<=126) {
        int first,last;
        if(glyph_bounds(ascii_glyphs[*s-32],&first,&last)) {
            if(offset+first<value_first) value_first=offset+first;
            value_last=offset+last;
        }
    }
    if(value_last<value_first) {centered_bitmap(frame,label,CENTER,label_y,0);return;}
    int label_width=label_last-label_first+1,value_width=value_last-value_first+1;
    int left=CENTER-(label_width+LABEL_GAP+value_width)/2;
    bitmap(frame,label,left-label_first,label_y,0);
    text(frame,value,left+label_width+LABEL_GAP-value_first,baseline,0);
}
static bool qr(uint8_t *frame,const char *payload,int center,int y)
{
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(6)],code[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
    if(!qrcodegen_encodeText(payload,temp,code,qrcodegen_Ecc_MEDIUM,1,6,qrcodegen_Mask_AUTO,true)) return false;
    int size=qrcodegen_getSize(code),scale=232/(size+8),x=center-size*scale/2;
    y+=(232-size*scale)/2;
    for(int j=0;j<size;j++) for(int i=0;i<size;i++) if(qrcodegen_getModule(code,i,j))
        for(int dy=0;dy<scale;dy++) for(int dx=0;dx<scale;dx++) pixel(frame,x+i*scale+dx,y+j*scale+dy,0);
    return true;
}
bool epaper_provision_render(uint8_t *frame,const char *ssid)
{
    if(!frame || !ssid || strlen(ssid)>32) return false;
    memset(frame,0x11,EPAPER_FRAME_BYTES);
    rect(frame,0,0,800,72,5);
    centered_bitmap(frame,&label_title,CENTER,18,1);
    centered_row(frame,&label_hotspot,ssid,111);
    centered_bitmap(frame,&label_wlan,LEFT_QR+QR_AREA/2,132,0);
    centered_bitmap(frame,&label_wechat,RIGHT_QR+QR_AREA/2,132,0);
    centered_bitmap(frame,&label_wlan_en,LEFT_QR+QR_AREA/2,154,0);
    centered_bitmap(frame,&label_wechat_en,RIGHT_QR+QR_AREA/2,154,0);
    char payload[96];snprintf(payload,sizeof(payload),"WIFI:T:nopass;S:%s;;",ssid);
    bool valid=qr(frame,payload,LEFT_QR+QR_AREA/2,176) &&
               qr(frame,EPAPER_PROVISION_URL,RIGHT_QR+QR_AREA/2,176);
    centered_bitmap(frame,&label_fallback,CENTER,416,0);
    centered_row(frame,&label_address,EPAPER_PROVISION_URL,469);
    return valid;
}

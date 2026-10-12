#include "epaper_album.h"
#include <stdlib.h>
#include <string.h>

static uint32_t random32(uint32_t *state)
{
    uint32_t x=*state?*state:0x93475acdu;
    x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return x;
}
static void deck(epaper_index_t *album,uint32_t seed,bool shuffle)
{
    for(size_t i=0;i<album->count;i++) album->deck[i]=(uint16_t)i;
    if(shuffle) {
        for(size_t i=album->count;i>1;i--) {
            size_t other=random32(&seed)%i;
            uint16_t t=album->deck[i-1];album->deck[i-1]=album->deck[other];album->deck[other]=t;
        }
    }
}
static void start_deck(epaper_catalog_t *c,epaper_player_t *s,unsigned a)
{
    deck(&c->albums[a],s->seeds[a],c->config.shuffle);
    if(c->config.shuffle && c->albums[a].count>1 && c->albums[a].deck[0]==s->previous[a]) {
        uint16_t t=c->albums[a].deck[0];c->albums[a].deck[0]=c->albums[a].deck[1];c->albums[a].deck[1]=t;
    }
}
void epaper_player_init(epaper_catalog_t *c,epaper_player_t *s,uint32_t seed)
{
    memset(s,0,sizeof(*s));s->magic=EPAPER_PLAYER_MAGIC;s->fingerprint=c->fingerprint;
    s->current_album=UINT32_MAX;s->current_photo=UINT32_MAX;
    for(unsigned i=0;i<c->config.album_count;i++) {
        s->previous[i]=UINT32_MAX;s->seeds[i]=random32(&seed);start_deck(c,s,i);
    }
}
bool epaper_player_valid(const epaper_catalog_t *c,const epaper_player_t *s)
{
    if(s->magic!=EPAPER_PLAYER_MAGIC || s->fingerprint!=c->fingerprint ||
       s->active>=c->config.album_count || s->current_album>=c->config.album_count ||
       s->current_photo>=c->albums[s->current_album].count ||
       !memchr(s->current_id,0,sizeof(s->current_id)) || !memchr(s->current_path,0,sizeof(s->current_path))) return false;
    for(unsigned i=0;i<c->config.album_count;i++) {
        if(s->cursor[i]>c->albums[i].count || !s->seeds[i] ||
           (s->previous[i]!=UINT32_MAX && s->previous[i]>=c->albums[i].count)) return false;
    }
    char path[EPAPER_PATH_MAX];
    return !strcmp(c->albums[s->current_album].definition.id,s->current_id) &&
        epaper_photo_path("",c,s->current_album,s->current_photo,path,sizeof(path)) && !strcmp(path,s->current_path);
}
bool epaper_player_restore(epaper_catalog_t *c,epaper_player_t *s,const epaper_player_t *saved)
{
    if(!saved || saved->magic!=EPAPER_PLAYER_MAGIC ||
       !memchr(saved->current_id,0,sizeof(saved->current_id)) ||
       !memchr(saved->current_path,0,sizeof(saved->current_path))) return false;
    if(epaper_player_valid(c,saved)) {
        *s=*saved;
        for(unsigned i=0;i<c->config.album_count;i++) start_deck(c,s,i);
        return true;
    }
    /* Index changed: preserve a still-valid displayed identity, rebuild the queue. */
    for(unsigned a=0;a<c->config.album_count;a++) {
        if(strcmp(c->albums[a].definition.id,saved->current_id)) continue;
        for(unsigned p=0;p<c->albums[a].count;p++) {
            char path[EPAPER_PATH_MAX];
            if(!epaper_photo_path("",c,a,p,path,sizeof(path)) || strcmp(path,saved->current_path)) continue;
            start_deck(c,s,a);
            for(size_t i=0;i<c->albums[a].count;i++) if(c->albums[a].deck[i]==p) {s->cursor[a]=(uint32_t)i+1;break;}
            s->current_album=a;s->current_photo=p;s->image_hash=saved->image_hash;
            strcpy(s->current_id,saved->current_id);strcpy(s->current_path,saved->current_path);
            s->active=c->config.strategy==ALBUM_ROUND_ROBIN?(a+1)%c->config.album_count:a;
            return true;
        }
    }
    return false;
}
bool epaper_player_pick(epaper_catalog_t *c,epaper_player_t *s,unsigned *album,unsigned *photo)
{
    if(!c->config.album_count || !c->total) return false;
    for(unsigned searched=0;searched<c->config.album_count;searched++) {
        unsigned a=s->active%c->config.album_count;
        epaper_index_t *index=&c->albums[a];
        if(index->count) {
            /* Recreate each deck so an uncommitted trial cannot affect live state. */
            start_deck(c,s,a);
            if(s->cursor[a]>=index->count) {
                s->previous[a]=index->deck[index->count-1];
                random32(&s->seeds[a]);s->cursor[a]=0;start_deck(c,s,a);
            }
            while(s->cursor[a]<index->count) {
                unsigned p=index->deck[s->cursor[a]++];
                if(index->photos[p].bad) continue;
                *album=a;*photo=p;
                if(c->config.strategy==ALBUM_ROUND_ROBIN || s->cursor[a]>=index->count)
                    s->active=(a+1)%c->config.album_count;
                return true;
            }
        }
        s->active=(a+1)%c->config.album_count;
    }
    return false;
}
void epaper_player_commit(epaper_catalog_t *c,epaper_player_t *s,unsigned album,unsigned photo,uint32_t hash)
{
    s->current_album=album;s->current_photo=photo;s->image_hash=hash;
    strcpy(s->current_id,c->albums[album].definition.id);
    epaper_photo_path("",c,album,photo,s->current_path,sizeof(s->current_path));
}

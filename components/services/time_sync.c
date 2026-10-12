#include "epaper_time_sync.h"
#include "epaper_runtime.h"
#include "epaper_devices.h"
#include "epaper_network.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG="time_sync";
static const epaper_time_config_t *settings;
static epaper_time_config_t owned_settings;
static nvs_handle_t store;
static bool utc,enabled,network_hold;
static epaper_time_status_t status;
bool epaper_time_enabled(void) {return enabled;}
void epaper_time_status(epaper_time_status_t *out) {if(out) {*out=status;out->enabled=enabled;}}
static RTC_DATA_ATTR struct {uint32_t magic,fingerprint;uint64_t next;} schedule;
static uint64_t wall_us(void)
{struct timeval t;gettimeofday(&t,NULL);return (uint64_t)t.tv_sec*1000000+(uint64_t)t.tv_usec;}
void epaper_time_start(const epaper_time_config_t *config,bool deep_wake)
{
    if(config) owned_settings=*config;
    settings=config?&owned_settings:NULL;enabled=config && config->enabled;
    if(nvs_open_from_partition("album_state","time",NVS_READWRITE,&store)!=ESP_OK) {enabled=false;return;}
    uint8_t marker=0;nvs_get_u8(store,"rtc_utc",&marker);utc=marker==1;
    nvs_get_i64(store,"last_sync",&status.last_success);
    size_t server_size=sizeof(status.last_server);
    nvs_get_str(store,"last_server",status.last_server,&server_size);
    uint32_t fp=config?epaper_hash(config,sizeof(*config),2166136261u):0;
    if(!deep_wake || schedule.magic!=0x4e545031 || schedule.fingerprint!=fp) {
        schedule.magic=0x4e545031;schedule.fingerprint=fp;schedule.next=0;
    }
    if(enabled) ESP_LOGI(TAG,"Periodic NTP enabled: interval=%lu s, retry=%lu s",(unsigned long)config->interval,(unsigned long)config->retry);
}
bool epaper_time_rtc_utc(void) {return utc;}
void epaper_time_update_network(const epaper_time_config_t *config)
{
    if(!config) return;
    bool was_enabled=enabled;enabled=config->enabled;
    owned_settings=*config;settings=&owned_settings;
    if(enabled && !was_enabled) schedule.next=0;
    schedule.fingerprint=epaper_hash(config,sizeof(*config),2166136261u);
}
void epaper_time_reconfigure(const epaper_time_config_t *config)
{
    epaper_time_update_network(config);
    if(config && enabled) schedule.next=wall_us()+(uint64_t)config->interval*1000000;
}
bool epaper_time_due(void) {return enabled && wall_us()>=schedule.next;}
uint64_t epaper_time_sleep_us(void)
{uint64_t now=wall_us();return enabled?(schedule.next>now?schedule.next-now:1000000):0;}
static bool matches(const epaper_rtc_t *rtc,time_t epoch)
{
    if(!rtc->valid) return false;
    for(int i=0;i<=2;i++) {
        struct tm t;time_t e=epoch+i;gmtime_r(&e,&t);
        const epaper_calendar_t *c=&rtc->calendar;
        if(c->year==t.tm_year+1900 && c->month==t.tm_mon+1 && c->day==t.tm_mday &&
           c->weekday==t.tm_wday && c->hour==t.tm_hour && c->minute==t.tm_min && c->second==t.tm_sec) return true;
    }
    return false;
}
static esp_err_t sync_once(void)
{
    esp_err_t err=epaper_network_start(settings);
    if(err!=ESP_OK) return err;
    ESP_LOGI(TAG,"Wi-Fi connected; requesting mainland NTP servers");
    uint64_t old_wall=wall_us();int64_t old_mono=esp_timer_get_time();
    err=ESP_ERR_TIMEOUT;
    const char *reply_server=NULL;
    for(unsigned i=0;i<settings->server_count;i++) {
        esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG(settings->servers[i]);
        esp_err_t attempt=esp_netif_sntp_init(&config);
        if(attempt==ESP_OK) {
            attempt=esp_netif_sntp_sync_wait(pdMS_TO_TICKS(settings->sync_timeout*1000/settings->server_count));
            esp_netif_sntp_deinit();
        }
        if(attempt==ESP_OK) {err=ESP_OK;reply_server=settings->servers[i];ESP_LOGI(TAG,"NTP reply from %s",reply_server);break;}
    }
    /* SNTP steps system time. Rebase album deadlines even if a reply arrived at timeout. */
    int64_t delta=(int64_t)wall_us()-(int64_t)old_wall-(esp_timer_get_time()-old_mono);
    epaper_album_clock_adjust(delta);
    if(err==ESP_OK) {
        time_t now=time(NULL);struct tm tm;gmtime_r(&now,&tm);
        if(tm.tm_year+1900<2024 || tm.tm_year+1900>2099) {err=ESP_ERR_INVALID_RESPONSE;goto cleanup;}
        epaper_calendar_t calendar={.year=tm.tm_year+1900,.month=tm.tm_mon+1,.day=tm.tm_mday,
            .weekday=tm.tm_wday,.hour=tm.tm_hour,.minute=tm.tm_min,.second=tm.tm_sec};
        /* Clear the persistent basis marker before touching the RTC. */
        err=nvs_set_u8(store,"rtc_utc",0);if(err==ESP_OK) err=nvs_commit(store);
        if(err!=ESP_OK) goto cleanup;
        utc=false;
        err=epaper_rtc_write(&calendar);
        epaper_rtc_t rtc;
        if(err==ESP_OK) err=epaper_rtc_read(&rtc);
        if(err==ESP_OK && !matches(&rtc,now)) err=ESP_ERR_INVALID_RESPONSE;
        if(err==ESP_OK) {
            err=nvs_set_u8(store,"rtc_utc",1);
            if(err==ESP_OK) err=nvs_set_i64(store,"last_sync",(int64_t)now);
            if(err==ESP_OK) err=nvs_set_str(store,"last_server",reply_server);
            if(err==ESP_OK) err=nvs_commit(store);
            if(err==ESP_OK) {
                utc=true;status.last_success=(int64_t)now;
                snprintf(status.last_server,sizeof(status.last_server),"%s",reply_server);
                time_t local=now+8*3600;gmtime_r(&local,&tm);
                ESP_LOGI(TAG,"RTC VERIFIED UTC; Beijing %04d-%02d-%02d %02d:%02d:%02d",
                    tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
            }
        }
    }
cleanup:
    if(!network_hold) epaper_network_stop();
    return err;
}
esp_err_t epaper_time_sync(void)
{
    if(!enabled) return ESP_ERR_INVALID_STATE;
    status.attempted=true;status.last_attempt=utc?(int64_t)time(NULL):0;
    esp_err_t err=sync_once();
    status.result=err;
    schedule.next=wall_us()+(uint64_t)(err==ESP_OK?settings->interval:settings->retry)*1000000;
    if(err!=ESP_OK) ESP_LOGW(TAG,"Sync failed: %s; retry in %lu seconds; album continues",esp_err_to_name(err),(unsigned long)settings->retry);
    return err;
}

void epaper_time_network_hold(bool hold) {network_hold=hold;}

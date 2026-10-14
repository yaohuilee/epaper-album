#include "provision_address.h"
#include "epaper_network.h"
#include "epaper_profile.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>

#define CONNECTED 1
#define FAILED 2
static esp_netif_t *netif,*ap_netif;
static bool provisioning;
static volatile bool connecting;
static char ap_ssid[33];
static int64_t finish_at;
static EventGroupHandle_t events;
static unsigned retries;
static bool system_ready,initialized,started,wifi_handler,ip_handler;
static volatile bool closing;
static esp_event_handler_instance_t wh,ih;
#ifdef CONFIG_EPAPER_DIAGNOSTICS
static portMUX_TYPE fault_lock=portMUX_INITIALIZER_UNLOCKED;
static int64_t fault_start_at,fault_end_at;
static bool fault_disconnected,fault_recovering;
bool epaper_network_fault_active(void)
{
    portENTER_CRITICAL(&fault_lock);bool active=fault_end_at!=0;portEXIT_CRITICAL(&fault_lock);return active;
}
esp_err_t epaper_network_fault_start(void)
{
    if(!epaper_profile_available()||!epaper_network_online()||provisioning)return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&fault_lock);
    if(fault_end_at){portEXIT_CRITICAL(&fault_lock);return ESP_ERR_INVALID_STATE;}
    fault_start_at=esp_timer_get_time()+1000000;fault_end_at=fault_start_at+15000000;fault_disconnected=fault_recovering=false;
    portEXIT_CRITICAL(&fault_lock);return ESP_OK;
}
void epaper_network_fault_tick(void)
{
    int64_t now=esp_timer_get_time();bool disconnect=false,reconnect=false,complete=false,online=epaper_network_online();
    portENTER_CRITICAL(&fault_lock);
    if(fault_end_at && now>=fault_start_at && !fault_disconnected){fault_disconnected=true;disconnect=true;}
    if(fault_end_at && now>=fault_end_at && !fault_recovering){fault_recovering=true;fault_end_at=now+10000000;reconnect=true;}
    else if(fault_end_at && fault_recovering && (online||now>=fault_end_at)){fault_start_at=fault_end_at=0;complete=true;}
    portEXIT_CRITICAL(&fault_lock);
    if(disconnect){connecting=false;epaper_profile_phase(PROFILE_FAULT,true);esp_wifi_disconnect();}
    if(reconnect){connecting=true;retries=0;esp_wifi_connect();}
    if(complete)epaper_profile_phase(PROFILE_FAULT,false);
}
#else
bool epaper_network_fault_active(void){return false;}
void epaper_network_fault_tick(void){}
esp_err_t epaper_network_fault_start(void){return ESP_ERR_NOT_SUPPORTED;}
#endif
static void event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    (void)arg;(void)data;
    if(closing || !events) return;
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        retries=0;xEventGroupClearBits(events,FAILED);xEventGroupSetBits(events,CONNECTED);
    } else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START && connecting) esp_wifi_connect();
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(events,CONNECTED);
        if(connecting && retries++<2) esp_wifi_connect();else xEventGroupSetBits(events,FAILED);
    } else if(base==IP_EVENT && id==IP_EVENT_STA_LOST_IP) xEventGroupClearBits(events,CONNECTED);
}
bool epaper_network_online(void)
{return events && (xEventGroupGetBits(events)&CONNECTED)!=0;}
bool epaper_network_ip(char *out,size_t size)
{
    if(!out || !size) return false;
    out[0]=0;esp_netif_ip_info_t info;
    if(!netif || !epaper_network_online() || esp_netif_get_ip_info(netif,&info)!=ESP_OK) return false;
    int n=snprintf(out,size,IPSTR,IP2STR(&info.ip));return n>0 && (size_t)n<size;
}
void epaper_network_stop(void)
{
    closing=true;connecting=false;
    epaper_captive_dns_stop();
    if(started) {esp_wifi_disconnect();esp_wifi_stop();}
    if(wifi_handler) esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wh);
    if(ip_handler) esp_event_handler_instance_unregister(IP_EVENT,ESP_EVENT_ANY_ID,ih);
    if(initialized) esp_wifi_deinit();
    if(netif) esp_netif_destroy_default_wifi(netif);
    if(ap_netif) esp_netif_destroy_default_wifi(ap_netif);
    if(events) vEventGroupDelete(events);
    if(initialized) ESP_LOGI("network","Wi-Fi stopped and deinitialized");
    netif=ap_netif=NULL;events=NULL;initialized=started=wifi_handler=ip_handler=provisioning=false;finish_at=0;
}
static esp_err_t initialize(void)
{
    epaper_network_stop();esp_err_t err;
    if(!system_ready) {
        err=esp_netif_init();if(err!=ESP_OK) return err;
        err=esp_event_loop_create_default();if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return err;
        system_ready=true;
    }
    esp_log_level_set("wifi",ESP_LOG_WARN);
    netif=esp_netif_create_default_wifi_sta();
    if(!netif) return ESP_ERR_NO_MEM;
    events=xEventGroupCreate();closing=false;retries=0;
    if(!events) {err=ESP_ERR_NO_MEM;goto fail;}
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();init.nvs_enable=0;
    err=esp_wifi_init(&init);if(err!=ESP_OK) goto fail;
    initialized=true;
    err=esp_wifi_set_ps(WIFI_PS_NONE);if(err!=ESP_OK) goto fail; /* Awake transfers must not wait for modem sleep. */
    err=esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,NULL,&wh);
    if(err!=ESP_OK) goto fail;
    wifi_handler=true;
    err=esp_event_handler_instance_register(IP_EVENT,ESP_EVENT_ANY_ID,event,NULL,&ih);
    if(err!=ESP_OK) goto fail;
    ip_handler=true;
    return ESP_OK;
fail:
    epaper_network_stop();return err;
}
esp_err_t epaper_network_start(const epaper_time_config_t *settings)
{
    if(!settings || !settings->enabled) return ESP_ERR_INVALID_STATE;
    if(!provisioning && epaper_network_online()) return ESP_OK;
    esp_err_t err=ESP_OK;
    if(!provisioning) {err=initialize();if(err!=ESP_OK) return err;}
    else {connecting=false;esp_wifi_disconnect();vTaskDelay(pdMS_TO_TICKS(100));xEventGroupClearBits(events,CONNECTED|FAILED);retries=0;finish_at=0;}
    wifi_config_t wifi={0};
    memcpy(wifi.sta.ssid,settings->ssid,strlen(settings->ssid));
    memcpy(wifi.sta.password,settings->password,strlen(settings->password));
    wifi.sta.threshold.authmode=settings->password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    err=esp_wifi_set_storage(WIFI_STORAGE_RAM);if(err!=ESP_OK) goto fail;
    err=esp_wifi_set_mode(provisioning?WIFI_MODE_APSTA:WIFI_MODE_STA);if(err!=ESP_OK) goto fail;
    err=esp_wifi_set_config(WIFI_IF_STA,&wifi);if(err!=ESP_OK) goto fail;
    connecting=true;
    err=started?esp_wifi_connect():esp_wifi_start();if(err!=ESP_OK) goto fail;
    started=true;
    EventBits_t bits=xEventGroupWaitBits(events,CONNECTED|FAILED,pdFALSE,pdFALSE,pdMS_TO_TICKS(settings->connect_timeout*1000));
    if(!(bits&CONNECTED)) {err=ESP_ERR_TIMEOUT;goto fail;}
    return ESP_OK;
fail:
    if(provisioning) {connecting=false;esp_wifi_disconnect();}else epaper_network_stop();return err;
}

esp_err_t epaper_network_scan(wifi_ap_record_t *records,uint16_t *count,uint16_t *total)
{
    if(!records || !count || !*count || !total) return ESP_ERR_INVALID_ARG;
    if(!started || (!epaper_network_online() && !provisioning)) return ESP_ERR_INVALID_STATE;
    wifi_scan_config_t scan={.show_hidden=false,.scan_type=WIFI_SCAN_TYPE_ACTIVE};
    esp_err_t err=esp_wifi_scan_start(&scan,true);
    if(err==ESP_OK) err=esp_wifi_scan_get_ap_num(total);
    if(err==ESP_OK) err=esp_wifi_scan_get_ap_records(count,records);
    if(err!=ESP_OK) esp_wifi_clear_ap_list();
    return err;
}

bool epaper_network_provisioning(void) {return provisioning;}
const char *epaper_network_ap_ssid(void) {return ap_ssid;}
esp_err_t epaper_network_provision_start(void)
{
    if(provisioning) return ESP_OK;
    esp_err_t err=initialize();if(err!=ESP_OK) return err;
    uint8_t mac[6];err=esp_read_mac(mac,ESP_MAC_WIFI_STA);if(err!=ESP_OK) goto fail;
    snprintf(ap_ssid,sizeof(ap_ssid),"epaper-album-%02X%02X%02X",mac[3],mac[4],mac[5]);
    ap_netif=esp_netif_create_default_wifi_ap();if(!ap_netif) {err=ESP_ERR_NO_MEM;goto fail;}
    /* DHCP advertises the local DNS endpoint and provisioning URI. */
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_ip_info_t info={0};
    err=esp_netif_str_to_ip4(EPAPER_PROVISION_IP,&info.ip);if(err!=ESP_OK) goto fail;
    info.gw=info.ip;
    err=esp_netif_str_to_ip4(EPAPER_PROVISION_NETMASK,&info.netmask);if(err!=ESP_OK) goto fail;
    err=esp_netif_set_ip_info(ap_netif,&info);if(err!=ESP_OK) goto fail;
    esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;dns.ip.u_addr.ip4=info.ip;
    err=esp_netif_set_dns_info(ap_netif,ESP_NETIF_DNS_MAIN,&dns);if(err!=ESP_OK) goto fail;
    uint8_t offer_dns=2;
    err=esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer_dns,sizeof(offer_dns));
    if(err!=ESP_OK) goto fail;
    static const char portal_uri[]=EPAPER_PROVISION_CAPTIVE_URI;
    err=esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,(void*)portal_uri,strlen(portal_uri));
    if(err!=ESP_OK) goto fail;
    err=esp_netif_dhcps_start(ap_netif);if(err!=ESP_OK) goto fail;
    wifi_config_t ap={0};memcpy(ap.ap.ssid,ap_ssid,strlen(ap_ssid));
    ap.ap.ssid_len=strlen(ap_ssid);ap.ap.channel=1;ap.ap.max_connection=4;ap.ap.authmode=WIFI_AUTH_OPEN;
    err=esp_wifi_set_storage(WIFI_STORAGE_RAM);if(err!=ESP_OK) goto fail;
    err=esp_wifi_set_mode(WIFI_MODE_APSTA);if(err!=ESP_OK) goto fail;
    err=esp_wifi_set_config(WIFI_IF_AP,&ap);if(err!=ESP_OK) goto fail;
    err=esp_wifi_start();if(err!=ESP_OK) goto fail;
    started=provisioning=true;connecting=false;
    for(unsigned i=0;i<50 && !esp_netif_is_netif_up(ap_netif);i++) vTaskDelay(pdMS_TO_TICKS(20));
    if(!esp_netif_is_netif_up(ap_netif)) {err=ESP_ERR_TIMEOUT;goto fail;}
    err=epaper_captive_dns_start();if(err!=ESP_OK) goto fail;
    ESP_LOGI("network","Provisioning AP %s; %s; no password",ap_ssid,EPAPER_PROVISION_URL);
    return ESP_OK;
fail:
    epaper_network_stop();return err;
}
void epaper_network_provision_complete(void)
{if(provisioning && epaper_network_online()) finish_at=esp_timer_get_time()+30000000;}
uint32_t epaper_network_provision_remaining_seconds(void)
{
    if(!provisioning || !finish_at) return 0;
    int64_t remaining=finish_at-esp_timer_get_time();
    return remaining>0?(uint32_t)((remaining+999999)/1000000):0;
}
bool epaper_network_provision_finish_due(void)
{return provisioning && finish_at && epaper_network_online() && esp_timer_get_time()>=finish_at;}
esp_err_t epaper_network_provision_finish(void)
{
    if(!provisioning || !epaper_network_online()) return ESP_ERR_INVALID_STATE;
    esp_err_t err=esp_wifi_set_mode(WIFI_MODE_STA);if(err!=ESP_OK) return err;
    epaper_captive_dns_stop();esp_netif_destroy_default_wifi(ap_netif);ap_netif=NULL;
    provisioning=false;finish_at=0;ESP_LOGI("network","Provisioning complete; AP stopped");return ESP_OK;
}

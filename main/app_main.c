#include "epaper_board.h"
#include "epaper_announcement.h"
#include "epaper_devices.h"
#include "epaper_storage.h"
#include "epaper_services.h"
#include "epaper_runtime.h"
#include "epaper_time_sync.h"
#include "epaper_network.h"
#include "epaper_profile.h"
#include "epaper_http.h"
#include "epaper_image_import.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG="app";
typedef enum { BUTTON_NEXT, BUTTON_SLEEP, BUTTON_KEY, BUTTON_PROVISION, BUTTON_PWR } button_event_t;
static QueueHandle_t events;
static epaper_usb_policy_t usb_policy;
static bool usb_debug,usb_reported,power_valid,power_vbus;
static int64_t network_retry_at;
/* Retain the last actual sleep conditions for charger/battery verification. */
static RTC_DATA_ATTR epaper_power_t previous_sleep_power;
static RTC_DATA_ATTR bool previous_sleep_valid;
static bool usb_awake(void)
{
    bool hold=epaper_usb_keep_awake(&usb_policy,usb_serial_jtag_is_connected(),esp_timer_get_time());
    if(!usb_reported || hold!=usb_debug) {
        ESP_LOGI(TAG,"USB host %s; %s",hold?"active":"inactive",
            hold?"debug keeps device awake":"no active data connection");
        usb_reported=true;usb_debug=hold;
    }
    return hold;
}
static void power_status(bool force)
{
    static int64_t next;
    static bool reported,valid;
    static uint8_t status1,status2;
    int64_t now=esp_timer_get_time();
    if(!force && now<next) return;
    next=now+2000000;
    epaper_power_t power;
    esp_err_t err=epaper_power_read(&power);
    if(err!=ESP_OK) {
        if(!reported || valid) ESP_LOGW(TAG,"Power status unavailable: %s",esp_err_to_name(err));
        reported=true;valid=false;power_valid=false;return;
    }
    if(!reported || !valid || power.status1!=status1 || power.status2!=status2) {
        const char *direction[]={"standby","charging","discharging","reserved"};
        const char *phase[]={"trickle","precharge","constant-current","constant-voltage","done","not-charging","reserved","reserved"};
        ESP_LOGI(TAG,"Power VBUS=%d battery=%d direction=%s phase=%s raw=%02x/%02x",
            power.vbus_good,power.battery_present,direction[power.current_direction],
            phase[power.charge_phase],power.status1,power.status2);
        status1=power.status1;status2=power.status2;
    }
    reported=true;valid=true;power_valid=true;power_vbus=power.vbus_good;
}
static bool keep_awake(void)
{
    power_status(false);
    return epaper_power_keep_awake(usb_awake(),power_valid,power_vbus);
}
static esp_err_t provision_start(void)
{
    epaper_http_stop();
    esp_err_t err=epaper_network_provision_start();
    if(err!=ESP_OK) return err;
    epaper_time_network_hold(true);
    err=epaper_provision_screen();
    if(err!=ESP_OK) ESP_LOGW(TAG,"Provisioning screen: %s",esp_err_to_name(err));
    return epaper_http_start();
}
static void web_status(bool awake)
{
    static int64_t update_at;
    int64_t now=esp_timer_get_time();
    bool portal=epaper_network_provisioning();
    epaper_time_network_hold(awake || portal);
    if(epaper_network_fault_active()) return;
    if(portal) {
        static int64_t report_at;
        if(now>=report_at) {
            ESP_LOGI(TAG,"Provisioning active; album refresh and sleep paused; station online=%d",epaper_network_online());
            report_at=now+30000000;
        }
        if(!epaper_http_running()) epaper_http_start();
        if(now>=update_at) {epaper_http_update(usb_debug);update_at=now+5000000;}
        return;
    }
    if(!awake) {
        epaper_http_stop();epaper_network_stop();network_retry_at=update_at=0;return;
    }
    if(!epaper_network_online() || !epaper_http_running()) {
        if(!epaper_announcement_admit(150))return;
        if(now<network_retry_at) return;
        epaper_http_stop();
        esp_err_t err=epaper_network_start(epaper_album_time_config());
        if(err==ESP_OK) err=epaper_http_start();
        if(err!=ESP_OK) {
            network_retry_at=esp_timer_get_time()+60000000;
            ESP_LOGW(TAG,"Status page unavailable: %s; retry in 60 seconds",esp_err_to_name(err));return;
        }
    }
    if(now>=update_at) {epaper_http_update(usb_debug);update_at=esp_timer_get_time()+5000000;}
}

/* Capture releases even during the slow display refresh. */
static void buttons(void *arg)
{
    (void)arg;
    const int pins[]={BOARD_BOOT,BOARD_KEY,BOARD_PWR_EVENT};
    int stable[3],candidate[3];int64_t changed[3],pressed_at[3];
    for(unsigned i=0;i<3;i++) {
        stable[i]=candidate[i]=gpio_get_level(pins[i]);changed[i]=esp_timer_get_time();
        pressed_at[i]=(i==2?stable[i]:!stable[i])?changed[i]:0;
    }
    for(;;) {
        int64_t now=esp_timer_get_time();
        for(unsigned i=0;i<3;i++) {
            int level=gpio_get_level(pins[i]);
            if(level!=candidate[i]) {candidate[i]=level;changed[i]=now;}
            if(level==stable[i] || now-changed[i]<40000) continue;
            stable[i]=level;
            if(i==2?level:!level) {pressed_at[i]=now;continue;}
            if(!pressed_at[i]) continue;
            bool long_press=now-pressed_at[i]>=2000000;
            button_event_t event=i==0?(long_press?BUTTON_SLEEP:BUTTON_NEXT):i==1?(long_press?BUTTON_PROVISION:BUTTON_KEY):BUTTON_PWR;
            ESP_LOGI(TAG,"%s %s release",i==0?"BOOT":i==1?"KEY":"PWR",long_press?"long":"short");
            xQueueSend(events,&event,0);pressed_at[i]=0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void diagnostics(void)
{
    epaper_rtc_t rtc;
    if(epaper_rtc_read(&rtc)==ESP_OK) {
        const epaper_calendar_t *c=&rtc.calendar;
        ESP_LOGI(TAG,"RTC %s %04d-%02d-%02d %02d:%02d:%02d valid=%d",
            epaper_time_rtc_utc()?"UTC":"unmigrated civil",c->year,c->month,c->day,c->hour,c->minute,c->second,rtc.valid);
    }
    epaper_environment_t env;
    if(epaper_environment_read(&env)==ESP_OK)
        ESP_LOGI(TAG,"SHTC3 %.2f C / %.2f %%RH",env.temperature_c,env.humidity_percent);
}
static bool released(void)
{ return gpio_get_level(BOARD_BOOT) && gpio_get_level(BOARD_KEY) && !gpio_get_level(BOARD_PWR_EVENT); }
static void sleep_now(void)
{
    power_status(true);
    if(keep_awake() || epaper_network_provisioning()) return; /* Also applies to manual BOOT sleep. */
    epaper_http_stop();epaper_network_stop();
    uint64_t delay=epaper_album_sleep_us();
    uint64_t announcement_delay=epaper_announcement_sleep_us();
    if(announcement_delay && (!delay || announcement_delay<delay))delay=announcement_delay;
    uint64_t time_delay=epaper_time_sleep_us();
    if(time_delay && (!delay || time_delay<delay)) delay=time_delay;
    esp_err_t err=esp_sleep_enable_ext1_wakeup_io((1ULL<<BOARD_BOOT)|(1ULL<<BOARD_KEY),ESP_EXT1_WAKEUP_ANY_LOW);
    if(err==ESP_OK && delay) err=esp_sleep_enable_timer_wakeup(delay);
    if(err!=ESP_OK) {ESP_LOGE(TAG,"Sleep setup failed: %s",esp_err_to_name(err));return;}
    rtc_gpio_pullup_en(BOARD_BOOT);rtc_gpio_pulldown_dis(BOARD_BOOT);
    rtc_gpio_pullup_en(BOARD_KEY);rtc_gpio_pulldown_dis(BOARD_KEY);
    ESP_LOGI(TAG,"Deep sleep; timer=%.1f seconds (0 means buttons only)",delay/1000000.0);
    previous_sleep_valid=epaper_power_read(&previous_sleep_power)==ESP_OK;
    if(!epaper_announcement_prepare_sleep())return;
    epaper_storage_unmount();epaper_album_prepare_sleep();
    esp_deep_sleep_start();
}
void app_main(void)
{
    bool deep=esp_reset_reason()==ESP_RST_DEEPSLEEP;
    uint64_t wake_keys=deep?esp_sleep_get_ext1_wakeup_status():0;
    rtc_gpio_deinit(BOARD_BOOT);rtc_gpio_deinit(BOARD_KEY);
    ESP_LOGI(TAG,"epaper-album %s; PSRAM=%u; deep_wake=%d",esp_app_get_description()->version,(unsigned)esp_psram_get_size(),deep);
    if(deep && previous_sleep_valid)
        ESP_LOGI(TAG,"Previous sleep: USB host=0 VBUS=%d battery=%d direction=%u phase=%u",
            previous_sleep_power.vbus_good,previous_sleep_power.battery_present,
            previous_sleep_power.current_direction,previous_sleep_power.charge_phase);
    esp_err_t err=epaper_board_init();
    if(err!=ESP_OK) {ESP_LOGE(TAG,"Board init failed: %s",esp_err_to_name(err));return;}
    if(epaper_environment_initialize()!=ESP_OK)ESP_LOGW(TAG,"Environment measurement lock unavailable");
    err=epaper_time_restore_rtc();
    if(err!=ESP_OK)ESP_LOGW(TAG,"RTC bootstrap unavailable: %s",esp_err_to_name(err));
    events=xQueueCreate(4,sizeof(button_event_t));
    if(!events || xTaskCreate(buttons,"buttons",3072,NULL,5,NULL)!=pdPASS) {
        ESP_LOGE(TAG,"Cannot start button capture");return;
    }
    if((wake_keys&(1ULL<<BOARD_BOOT)) && gpio_get_level(BOARD_BOOT)) {
        button_event_t e=BUTTON_NEXT;xQueueSend(events,&e,0);
    }
    err=epaper_storage_mount();
    bool running=false;
    if(err==ESP_OK) {
        esp_err_t provision=epaper_wifi_provision();
        if(provision!=ESP_OK) ESP_LOGW(TAG,"Wi-Fi provisioning failed: %s",esp_err_to_name(provision));
        err=epaper_album_start(deep);
        running=err==ESP_OK || err==ESP_ERR_NOT_FOUND;
        if(running)epaper_image_cleanup();
    }
    if(!running) ESP_LOGE(TAG,"Album startup: %s; awake recovery mode, restart after fixing SD/config",esp_err_to_name(err));
    epaper_profile_start();
    epaper_time_start(running?epaper_album_time_config():NULL,deep);
    esp_err_t audio=running?epaper_announcement_start():ESP_ERR_INVALID_STATE;
    if(audio!=ESP_OK)ESP_LOGW(TAG,"Announcement service: %s",esp_err_to_name(audio));
    bool awake_boot=keep_awake();
    epaper_time_network_hold(true);
    bool attempted=epaper_time_due();
    if(attempted && epaper_announcement_admit(150)) {
        err=epaper_time_sync();
        if(err!=ESP_OK && !epaper_network_online()) network_retry_at=esp_timer_get_time()+60000000;
    }
    const epaper_time_config_t *network=epaper_album_time_config();
    /* Only an absent network configuration opens the automatic setup screen.
       A saved network can be unavailable indefinitely without stopping photos. */
    if(epaper_network_auto_provision(network && network->enabled,network?network->ssid:NULL)) {
        if(epaper_announcement_admit(150)) {
            err=provision_start();
            if(err!=ESP_OK) ESP_LOGE(TAG,"Provisioning failed: %s",esp_err_to_name(err));
        }
    } else if(epaper_announcement_admit(150) && !epaper_network_online() &&
              esp_timer_get_time()>=network_retry_at && (!deep || awake_boot || attempted)) {
        err=epaper_network_start(network);
        if(err!=ESP_OK) {
            network_retry_at=esp_timer_get_time()+60000000;
            ESP_LOGW(TAG,"Saved Wi-Fi unavailable: %s; album continues",esp_err_to_name(err));
        }
    }
    diagnostics();
    bool queued_next=false,queued_provision=false;
    int64_t idle_until=esp_timer_get_time()+(int64_t)CONFIG_EPAPER_IDLE_SECONDS*1000000;
    bool manual_sleep=false;
    unsigned next_revision=epaper_http_next_revision();
    for(;;) {
        bool awake=keep_awake();
        web_status(awake);
        if(epaper_http_process()) idle_until=esp_timer_get_time()+(int64_t)CONFIG_EPAPER_IDLE_SECONDS*1000000;
        bool http_advanced=next_revision!=epaper_http_next_revision();
        if(http_advanced){next_revision=epaper_http_next_revision();queued_next=false;}
        if(awake) manual_sleep=false;
        if(!epaper_network_provisioning() && !epaper_network_fault_active() && epaper_announcement_admit(150) && epaper_time_due()) {
            err=epaper_time_sync();
            if(err!=ESP_OK && !epaper_network_online()) network_retry_at=esp_timer_get_time()+60000000;
            idle_until=esp_timer_get_time()+(int64_t)CONFIG_EPAPER_IDLE_SECONDS*1000000;
        }
        button_event_t e;bool next=false;
        if(!epaper_http_sd_try_lock()) {vTaskDelay(pdMS_TO_TICKS(20));continue;}
        /* Coalesce presses accumulated during one full refresh. */
        while(xQueueReceive(events,&e,0)==pdTRUE) {
            idle_until=esp_timer_get_time()+(int64_t)CONFIG_EPAPER_IDLE_SECONDS*1000000;
            if(e==BUTTON_SLEEP) {
                if(awake) ESP_LOGI(TAG,"BOOT sleep ignored while external power or USB host is active, or power status is unknown");
                else manual_sleep=true;
            }
            else if(e==BUTTON_NEXT) {if(!http_advanced && !epaper_network_provisioning()) next=true;manual_sleep=false;}
            else if(e==BUTTON_PROVISION) {
                if(!epaper_announcement_admit(250)){queued_provision=true;ESP_LOGI(TAG,"Provisioning request queued until announcement completes");continue;}
                manual_sleep=false;err=provision_start();
                if(err!=ESP_OK) ESP_LOGW(TAG,"Provisioning: %s",esp_err_to_name(err));
            }
            else if(e==BUTTON_KEY) {
                manual_sleep=false;diagnostics();
                ESP_LOGI(TAG,"KEY wake/diagnostics");
            } else ESP_LOGI(TAG,"PWR event; long hardware shutdown remains PMIC-controlled");
        }
        if(epaper_network_provision_finish_due() && epaper_announcement_admit(250)) {
            err=epaper_network_provision_finish();
            if(err==ESP_OK && running) err=epaper_album_restore_current();
            if(err!=ESP_OK) ESP_LOGW(TAG,"Provisioning finish: %s",esp_err_to_name(err));
            idle_until=esp_timer_get_time()+(int64_t)CONFIG_EPAPER_IDLE_SECONDS*1000000;
        }
        if(next)queued_next=true;
        if(queued_provision && epaper_announcement_admit(250)){
            queued_provision=false;manual_sleep=false;err=provision_start();
            if(err!=ESP_OK)ESP_LOGW(TAG,"Deferred provisioning: %s",esp_err_to_name(err));
        }
        if(running && !epaper_network_provisioning() && epaper_announcement_admit(250) && (queued_next || (!manual_sleep && epaper_album_due()))) {
            queued_next=false;
            epaper_profile_phase(PROFILE_PHOTO,true);
            err=epaper_album_next();
            epaper_profile_phase(PROFILE_PHOTO,false);
            if(err!=ESP_OK) ESP_LOGW(TAG,"Advance: %s",esp_err_to_name(err));
            idle_until=esp_timer_get_time()+(int64_t)CONFIG_EPAPER_IDLE_SECONDS*1000000;
        }
        if(running && !epaper_network_provisioning() && !queued_next && !queued_provision && !epaper_announcement_busy() && !epaper_image_active() && !awake && released() && (manual_sleep ||
           (epaper_album_can_sleep() && esp_timer_get_time()>=idle_until))) sleep_now();
        epaper_http_sd_unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

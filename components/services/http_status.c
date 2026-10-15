#include "provision_address.h"
#include "epaper_announcement.h"
#include "epaper_http.h"
#include "epaper_http_internal.h"
#include "epaper_network.h"
#include "epaper_profile.h"
#include "epaper_image_import.h"
#include "epaper_runtime.h"
#include "epaper_services.h"
#include "epaper_time_sync.h"
#include "epaper_devices.h"
#include "epaper_storage.h"
#include "esp_http_server.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static httpd_handle_t server;
static SemaphoreHandle_t lock;
static char *snapshot;
static int64_t snapshot_rtc_sample_us;
static char token[33],pending_body[4097],job_error[160];
static bool last_usb_host;
static unsigned job_id,next_revision;
unsigned epaper_http_next_revision(void){return next_revision;}
static bool pending,running,settings_job,rescan_job,time_job,time_settings_job,wifi_job,wifi_scan_job,provision_job,album_create_job,album_delete_job,album_rename_job,announcement_job,announcement_pause_job,announcement_test_job;
static char *wifi_scan_result;
static const char *job_state="idle";
static SemaphoreHandle_t sd_lock;
static bool files_busy,index_pending,index_running;
static unsigned files_revision;
static char index_error[160];
bool epaper_http_sd_try_lock(void)
{
    /* First initialized by app_main before any HTTP handlers can run. */
    if(!sd_lock) sd_lock=xSemaphoreCreateMutex();
    return sd_lock && xSemaphoreTake(sd_lock,0)==pdTRUE;
}
void epaper_http_sd_unlock(void) {xSemaphoreGive(sd_lock);}
bool epaper_http_authorized(httpd_req_t *req)
{
    char supplied[40];
    return httpd_req_get_hdr_value_str(req,"X-Album-Token",supplied,sizeof(supplied))==ESP_OK &&
        !strcmp(supplied,token);
}
bool epaper_http_file_available(unsigned seconds)
{
    if(epaper_network_provisioning() || !epaper_announcement_admit(seconds) || !epaper_http_sd_try_lock())return false;
    xSemaphoreTake(lock,portMAX_DELAY);bool available=!(pending||running||files_busy||index_pending||index_running);xSemaphoreGive(lock);
    if(available && epaper_album_due() && epaper_announcement_admit(250))available=false;
    epaper_http_sd_unlock();return available;
}
bool epaper_http_file_begin_for(unsigned seconds)
{
    if(epaper_network_provisioning() || !epaper_announcement_admit(seconds) || !epaper_http_sd_try_lock()) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool available=!(pending||running||files_busy||index_pending||index_running);
    if(available) files_busy=true;
    xSemaphoreGive(lock);
    if(!available) epaper_http_sd_unlock();
    else epaper_profile_phase(PROFILE_TRANSFER,true);
    return available;
}
bool epaper_http_file_begin(void){return epaper_http_file_begin_for(75);}
unsigned epaper_http_file_revision(void){xSemaphoreTake(lock,portMAX_DELAY);unsigned revision=files_revision;xSemaphoreGive(lock);return revision;}
bool epaper_http_index_ready(unsigned revision,char *error,size_t size)
{
    xSemaphoreTake(lock,portMAX_DELAY);bool ready=files_revision>=revision && !index_pending && !index_running;
    if(ready)snprintf(error,size,"%s",index_error);
    xSemaphoreGive(lock);return ready;
}
void epaper_http_file_end(bool changed)
{
    epaper_profile_phase(PROFILE_TRANSFER,false);
    xSemaphoreTake(lock,portMAX_DELAY);files_busy=false;
    if(changed) {files_revision++;index_pending=true;index_error[0]=0;}
    xSemaphoreGive(lock);epaper_http_sd_unlock();
}
extern const char page[] asm("_binary_status_html_start");
extern const char setup_page[] asm("_binary_setup_html_start");
extern const char album_page[] asm("_binary_albums_html_start");
extern const char ui_css[] asm("_binary_ui_css_start");
extern const char ui_js[] asm("_binary_ui_js_start");
extern const char announcement_js[] asm("_binary_announcements_js_start");
static esp_err_t ui_asset(httpd_req_t *req)
{
    bool css=!strncmp(req->uri,"/ui.css",7);
    httpd_resp_set_type(req,css?"text/css; charset=utf-8":"text/javascript; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    return httpd_resp_send(req,css?ui_css:!strcmp(req->uri,"/announcements.js")?announcement_js:ui_js,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t captive_redirect(httpd_req_t *req,httpd_err_code_t code)
{
    (void)code;
    if(!epaper_network_provisioning()) return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Not found");
    httpd_resp_set_status(req,"302 Found");httpd_resp_set_hdr(req,"Location",EPAPER_PROVISION_URL);
    httpd_resp_set_hdr(req,"Cache-Control","no-store");return httpd_resp_send(req,NULL,0);
}
static esp_err_t setup_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,setup_page,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t captive_api(httpd_req_t *req)
{
    httpd_resp_set_type(req,"application/captive+json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,epaper_network_provisioning()?
        "{\"captive\":true,\"user-portal-url\":\"" EPAPER_PROVISION_URL "\"}":
        "{\"captive\":false}",HTTPD_RESP_USE_STRLEN);
}
static esp_err_t albums_page(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    return httpd_resp_send(req,album_page,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t index_page(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    return httpd_resp_send(req,epaper_network_provisioning()?setup_page:page,HTTPD_RESP_USE_STRLEN);
}
#ifdef CONFIG_EPAPER_DIAGNOSTICS
static esp_err_t diagnostics(httpd_req_t *req)
{
    if(!epaper_http_authorized(req)) {httpd_resp_set_status(req,"403 Forbidden");return httpd_resp_sendstr(req,"Unauthorized");}
    if(req->method==HTTP_POST) {
        char body[3]={0};size_t received=0;
        if(req->content_len!=2){httpd_resp_set_status(req,"400 Bad Request");return httpd_resp_sendstr(req,"Use empty JSON object");}
        while(received<2){int n=httpd_req_recv(req,body+received,2-received);if(n<=0)return ESP_FAIL;received+=(size_t)n;}
        if(strcmp(body,"{}")){httpd_resp_set_status(req,"400 Bad Request");return httpd_resp_sendstr(req,"Use empty JSON object");}
        bool fault=!strcmp(req->uri,"/api/diagnostics/network-fault");
        if(fault && !last_usb_host){httpd_resp_set_status(req,"409 Conflict");return httpd_resp_sendstr(req,"Network fault requires USB host");}
        bool ok=fault?epaper_network_fault_start()==ESP_OK:epaper_profile_reset();
        if(!ok){httpd_resp_set_status(req,"409 Conflict");return httpd_resp_sendstr(req,"Profiler or network busy");}
    }
    cJSON *root=cJSON_CreateObject();if(!root)return ESP_ERR_NO_MEM;
    epaper_profile_json(root);char *json=cJSON_PrintUnformatted(root);cJSON_Delete(root);
    if(!json)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(req,json);free(json);return err;
}
#endif
static esp_err_t status_json(httpd_req_t *req)
{
    xSemaphoreTake(lock,portMAX_DELAY);
    char *copy=snapshot?strdup(snapshot):NULL;
    int64_t sample_us=snapshot_rtc_sample_us;
    xSemaphoreGive(lock);
    if(!copy) {
        httpd_resp_set_status(req,"503 Service Unavailable");
        return httpd_resp_send(req,"Status unavailable",HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    char age[24];int64_t elapsed_us=esp_timer_get_time()-sample_us;
    snprintf(age,sizeof(age),"%lld",(long long)(elapsed_us>0?elapsed_us/1000:0));
    httpd_resp_set_hdr(req,"X-Status-Age-Ms",age);
    esp_err_t err=httpd_resp_send(req,copy,HTTPD_RESP_USE_STRLEN);free(copy);return err;
}
static esp_err_t command_error(httpd_req_t *req,const char *status,const char *message)
{
    httpd_resp_set_status(req,status);
    httpd_resp_set_type(req,"text/plain; charset=utf-8");
    return httpd_resp_send(req,message,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t wifi_scan_result_json(httpd_req_t *req)
{
    if(!epaper_http_authorized(req)) return command_error(req,"403 Forbidden","Refresh the device page first");
    xSemaphoreTake(lock,portMAX_DELAY);
    char pending_scan[96];char *copy=NULL;
    if(wifi_scan_job && (pending || running)) {
        snprintf(pending_scan,sizeof(pending_scan),"{\"job_id\":%u,\"state\":\"%s\"}",job_id,job_state);
        copy=strdup(pending_scan);
    } else if(wifi_scan_job && !wifi_scan_result) {
        snprintf(pending_scan,sizeof(pending_scan),"{\"job_id\":%u,\"state\":\"failed\",\"error\":\"Scan result unavailable\"}",job_id);copy=strdup(pending_scan);
    } else copy=strdup(wifi_scan_result?wifi_scan_result:"{\"state\":\"idle\"}");
    xSemaphoreGive(lock);
    if(!copy) return command_error(req,"503 Service Unavailable","Out of memory");
    httpd_resp_set_type(req,"application/json; charset=utf-8");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_send(req,copy,HTTPD_RESP_USE_STRLEN);free(copy);return err;
}
static esp_err_t scan_wifi_networks(void)
{
    const uint16_t capacity=64;uint16_t count=capacity,total=0;
    wifi_ap_record_t *records=calloc(capacity,sizeof(*records));
    esp_err_t err=records?epaper_network_scan(records,&count,&total):ESP_ERR_NO_MEM;
    cJSON *doc=cJSON_CreateObject(),*networks=doc?cJSON_AddArrayToObject(doc,"networks"):NULL;
    if(!doc || !networks) {free(records);cJSON_Delete(doc);return ESP_ERR_NO_MEM;}
    cJSON_AddNumberToObject(doc,"job_id",job_id);cJSON_AddStringToObject(doc,"state",err==ESP_OK?"succeeded":"failed");
    cJSON_AddStringToObject(doc,"error",err==ESP_OK?"":esp_err_to_name(err));
    cJSON_AddNumberToObject(doc,"total",total);cJSON_AddBoolToObject(doc,"truncated",total>capacity);
    for(uint16_t i=0;err==ESP_OK && i<count;i++) {
        wifi_ap_record_t *ap=&records[i];char ssid[33];memcpy(ssid,ap->ssid,32);ssid[32]=0;
        if(!ssid[0]) continue;
        cJSON *item=cJSON_CreateObject();if(!item) {err=ESP_ERR_NO_MEM;break;}
        cJSON_AddStringToObject(item,"ssid",ssid);cJSON_AddNumberToObject(item,"rssi_dbm",ap->rssi);
        cJSON_AddNumberToObject(item,"channel",ap->primary);cJSON_AddNumberToObject(item,"authmode",ap->authmode);
        cJSON_AddBoolToObject(item,"open",ap->authmode==WIFI_AUTH_OPEN);cJSON_AddItemToArray(networks,item);
    }
    free(records);char *text=err==ESP_ERR_NO_MEM?NULL:cJSON_PrintUnformatted(doc);cJSON_Delete(doc);
    xSemaphoreTake(lock,portMAX_DELAY);free(wifi_scan_result);wifi_scan_result=text;xSemaphoreGive(lock);
    return text?err:ESP_ERR_NO_MEM;
}
static esp_err_t command(httpd_req_t *req)
{
    char type[64];
    if(!epaper_http_authorized(req)) return command_error(req,"403 Forbidden","Refresh the device page first");
    if(httpd_req_get_hdr_value_str(req,"Content-Type",type,sizeof(type))!=ESP_OK ||
       strcmp(type,"application/json")) return command_error(req,"415 Unsupported Media Type","Use application/json");
    if(req->content_len>4096) return command_error(req,"413 Content Too Large","Request too large");
    char *body=malloc(req->content_len+1);if(!body) return ESP_ERR_NO_MEM;
    size_t received=0;
    while(received<req->content_len) {
        int n=httpd_req_recv(req,body+received,req->content_len-received);
        if(n<=0) {free(body);return command_error(req,"400 Bad Request","Incomplete request");}
        received+=(size_t)n;
    }
    body[received]=0;
    if(memchr(body,0,received)) {free(body);return command_error(req,"400 Bad Request","Invalid JSON body");}
    bool settings=!strcmp(req->uri,"/api/settings");
    bool album_create=!strcmp(req->uri,"/api/album-create"),album_delete=!strcmp(req->uri,"/api/album-delete"),album_rename=!strcmp(req->uri,"/api/album-rename");
    if((album_create||album_delete||album_rename) && epaper_image_active()){free(body);return command_error(req,"409 Conflict","Photo conversion is active; retry after completion");}
    bool time_settings=!strcmp(req->uri,"/api/time-settings");
    bool wifi_settings=!strcmp(req->uri,"/api/wifi-settings");
    bool announcement=!strcmp(req->uri,"/api/announcement-settings"),announcement_pause=!strcmp(req->uri,"/api/announcement-pause-settings"),announcement_test=!strcmp(req->uri,"/api/announcement-test");
    if(strcmp(req->uri,"/api/next") && !epaper_announcement_admit(150)){free(body);return command_error(req,"409 Conflict","Announcement is busy or imminent; retry later");}
    if(epaper_network_provisioning() && !wifi_settings && strcmp(req->uri,"/api/wifi-scan")) {
        free(body);return command_error(req,"409 Conflict","Provisioning mode accepts Wi-Fi commands only");
    }
    if(!settings && !time_settings && !wifi_settings && !album_create && !album_delete && !album_rename && !announcement && !announcement_pause && !announcement_test && strcmp(body,"{}")) {free(body);return command_error(req,"400 Bad Request","Expected empty JSON object");}
    xSemaphoreTake(lock,portMAX_DELAY);
    if(pending || running || files_busy || index_pending || index_running) {
        xSemaphoreGive(lock);free(body);
        return command_error(req,"409 Conflict","A command is already in progress");
    }
    bool sync=!strcmp(req->uri,"/api/time-sync");
    if((sync || time_settings || (wifi_settings && !epaper_network_provisioning())) && !epaper_time_enabled()) {xSemaphoreGive(lock);free(body);return command_error(req,"409 Conflict","Time synchronization is disabled");}
    strcpy(pending_body,body);memset(body,0,received);free(body);settings_job=settings;rescan_job=!strcmp(req->uri,"/api/rescan");time_job=sync;time_settings_job=time_settings;wifi_job=wifi_settings;wifi_scan_job=!strcmp(req->uri,"/api/wifi-scan");provision_job=!strcmp(req->uri,"/api/wifi-provision");if(wifi_scan_job) {free(wifi_scan_result);wifi_scan_result=NULL;}album_create_job=album_create;album_delete_job=album_delete;album_rename_job=album_rename;announcement_job=announcement;announcement_pause_job=announcement_pause;announcement_test_job=announcement_test;pending=true;
    job_id++;job_state="queued";job_error[0]=0;unsigned id=job_id;
    xSemaphoreGive(lock);
    char response[80];snprintf(response,sizeof(response),"{\"job_id\":%u,\"state\":\"queued\"}",id);
    httpd_resp_set_status(req,"202 Accepted");
    httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,response,HTTPD_RESP_USE_STRLEN);
}
static char *wifi_rollback_request(void)
{
    const epaper_time_config_t *previous=epaper_album_time_config();
    if(!previous) return NULL;
    cJSON *doc=cJSON_CreateObject();if(!doc) return NULL;
    char *text=NULL;
    if(cJSON_AddNumberToObject(doc,"schema_version",1) &&
       cJSON_AddStringToObject(doc,"ssid",previous->ssid) &&
       cJSON_AddStringToObject(doc,"password",previous->password)) text=cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);return text;
}
static esp_err_t begin_provisioning(void)
{
    /* Complete the 202 handler before removing its station interface. */
    vTaskDelay(pdMS_TO_TICKS(300));epaper_http_stop();
    esp_err_t err=epaper_network_provision_start();
    if(err!=ESP_OK) return err;
    epaper_time_network_hold(true);
    err=epaper_provision_screen();
    if(err!=ESP_OK) ESP_LOGW("http","Provisioning screen failed: %s",esp_err_to_name(err));
    return err;
}
bool epaper_http_process(void)
{
    if(!lock || !epaper_http_sd_try_lock()) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool reindex=index_pending;
    if(!pending && !reindex) {xSemaphoreGive(lock);epaper_http_sd_unlock();return false;}
    unsigned budget=(!reindex && (announcement_job||announcement_pause_job||announcement_test_job))?55:250;
    if(!epaper_announcement_admit(budget)){xSemaphoreGive(lock);epaper_http_sd_unlock();return false;}
    if(reindex) {index_pending=false;index_running=true;}
    else {pending=false;running=true;job_state="running";}
    bool settings=settings_job,force_rescan=rescan_job,sync=time_job,time_settings=time_settings_job,wifi=wifi_job,scan=wifi_scan_job,provision=provision_job,create=album_create_job,remove_album=album_delete_job,rename_album=album_rename_job;
    xSemaphoreGive(lock);
    epaper_http_sd_unlock();
    epaper_http_update(last_usb_host);
    /* Transfer admission sees the running flag, so this cannot wait on network I/O. */
    xSemaphoreTake(sd_lock,portMAX_DELAY);
    char error[160]={0};
    bool portal=epaper_network_provisioning();
    char *rollback=NULL;
    if(wifi && !reindex) {
        if(portal) epaper_json_read(EPAPER_SD_MOUNT CONFIG_EPAPER_CONFIG_PATH,&rollback);
        else rollback=wifi_rollback_request();
    }
    bool photo_job=!reindex && !force_rescan && !announcement_job && !announcement_pause_job && !announcement_test_job && !sync && !provision && !scan && !wifi && !time_settings && !settings && !create && !remove_album && !rename_album;
    if(photo_job)epaper_profile_phase(PROFILE_PHOTO,true);
    esp_err_t err=reindex||force_rescan?epaper_album_rescan():announcement_job?epaper_announcement_settings(pending_body,false,error,sizeof(error)):announcement_pause_job?epaper_announcement_settings(pending_body,true,error,sizeof(error)):announcement_test_job?epaper_announcement_test(pending_body,error,sizeof(error)):sync?epaper_time_sync():provision?begin_provisioning():scan?scan_wifi_networks():wifi?(rollback?epaper_album_wifi_settings(pending_body,error,sizeof(error)):ESP_ERR_NO_MEM):time_settings?epaper_album_time_settings(pending_body,error,sizeof(error)):settings?epaper_album_settings(pending_body,error,sizeof(error)):rename_album?epaper_album_rename(pending_body,error,sizeof(error)):(create||remove_album)?epaper_album_manage(pending_body,remove_album,error,sizeof(error)):epaper_album_next();
    if(photo_job)epaper_profile_phase(PROFILE_PHOTO,false);
    if(time_settings && !reindex && err==ESP_OK) epaper_time_reconfigure(epaper_album_time_config());
    bool reconnect=wifi && !reindex && err==ESP_OK;
    if(reconnect) {
        epaper_time_update_network(epaper_album_time_config());
        /* The command handler has already returned 202; stop waits for handlers
           to finish before tearing down the station and its old IP address. */
        if(!portal) {epaper_http_stop();epaper_network_stop();}
        err=epaper_network_start(epaper_album_time_config());
        if(err==ESP_OK && portal) epaper_network_provision_complete();
        if(err!=ESP_OK && portal) {
            esp_err_t restored=epaper_album_wifi_restore_root(rollback);
            if(restored==ESP_OK) epaper_time_update_network(epaper_album_time_config());
            snprintf(error,sizeof(error),restored==ESP_OK?"Connection failed; original SD Wi-Fi restored; provisioning AP remains available":"Connection failed; SD rollback failed; provisioning AP remains available");
        }
        if(err!=ESP_OK && !portal) {
            esp_err_t connection_error=err;
            char restore_error[160]={0};
            esp_err_t restored=epaper_album_wifi_settings(rollback,restore_error,sizeof(restore_error));
            if(restored==ESP_OK) {
                epaper_time_update_network(epaper_album_time_config());
                esp_err_t connected=epaper_network_start(epaper_album_time_config());
                snprintf(error,sizeof(error),"New network failed (%s); original SD Wi-Fi restored; %s",
                    esp_err_to_name(connection_error),connected==ESP_OK?"original network reconnected":"original network unavailable");
            } else snprintf(error,sizeof(error),"New network failed; SD rollback failed (%s); saved settings require correction",esp_err_to_name(restored));
            err=connection_error;
        }
    }
    if(rollback) {memset(rollback,0,strlen(rollback));free(rollback);}
    if(!reindex) memset(pending_body,0,sizeof(pending_body));
    if(err==ESP_OK && !reindex && !force_rescan && !settings && !sync && !time_settings && !wifi && !scan && !provision && !create && !remove_album && !announcement_job && !announcement_pause_job && !announcement_test_job)next_revision++;
    if(err!=ESP_OK && !error[0]) snprintf(error,sizeof(error),"Command failed: %s",esp_err_to_name(err));
    xSemaphoreTake(lock,portMAX_DELAY);
    if(reindex) {index_running=false;snprintf(index_error,sizeof(index_error),"%s",error);}
    else {running=false;job_state=err==ESP_OK?"succeeded":"failed";snprintf(job_error,sizeof(job_error),"%s",error);}
    if(force_rescan) snprintf(index_error,sizeof(index_error),"%s",error);
    xSemaphoreGive(lock);
    epaper_http_sd_unlock();
    epaper_http_update(last_usb_host);
    if(reconnect && epaper_network_online()) epaper_http_start();
    return true;
}
bool epaper_http_running(void) {return server!=NULL;}
void epaper_http_stop(void)
{
    if(server) {httpd_stop(server);server=NULL;ESP_LOGI("http","Status server stopped");}
    if(lock) {
        xSemaphoreTake(lock,portMAX_DELAY);free(snapshot);snapshot=NULL;xSemaphoreGive(lock);
    }
}
esp_err_t epaper_http_start(void)
{
    if(server) return ESP_OK;
    if(!epaper_network_online() && !epaper_network_provisioning()) return ESP_ERR_INVALID_STATE;
    if(!lock) lock=xSemaphoreCreateMutex();
    if(!lock) return ESP_ERR_NO_MEM;
    if(!token[0]) for(unsigned i=0;i<4;i++) snprintf(token+i*8,9,"%08lx",(unsigned long)esp_random());
    epaper_http_update(last_usb_host);
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.stack_size=8192;config.max_uri_handlers=48;config.max_open_sockets=4;config.lru_purge_enable=true;
    /* A weak WLAN can pause during a large BMP transfer. Keep the overall
       file-transfer deadline, but allow individual socket waits to recover. */
    config.recv_wait_timeout=20;config.send_wait_timeout=20;
    esp_err_t err=httpd_start(&server,&config);
    if(err!=ESP_OK) return err;
    const httpd_uri_t setup={.uri="/setup",.method=HTTP_GET,.handler=setup_handler};
    const httpd_uri_t captive_uri={.uri="/api/captive-portal",.method=HTTP_GET,.handler=captive_api};
    const httpd_uri_t index={.uri="/",.method=HTTP_GET,.handler=index_page};
    const httpd_uri_t album_ui={.uri="/albums",.method=HTTP_GET,.handler=albums_page};
    const httpd_uri_t css={.uri="/ui.css",.method=HTTP_GET,.handler=ui_asset};
    const httpd_uri_t js={.uri="/ui.js",.method=HTTP_GET,.handler=ui_asset};
    const httpd_uri_t status={.uri="/api/status",.method=HTTP_GET,.handler=status_json};
    const httpd_uri_t next={.uri="/api/next",.method=HTTP_POST,.handler=command};
    const httpd_uri_t settings={.uri="/api/settings",.method=HTTP_POST,.handler=command};
    const httpd_uri_t rescan={.uri="/api/rescan",.method=HTTP_POST,.handler=command};
    const httpd_uri_t time_sync={.uri="/api/time-sync",.method=HTTP_POST,.handler=command};
    const httpd_uri_t time_settings={.uri="/api/time-settings",.method=HTTP_POST,.handler=command};
    const httpd_uri_t wifi_scan={.uri="/api/wifi-scan",.method=HTTP_POST,.handler=command};
    const httpd_uri_t wifi_scan_result_uri={.uri="/api/wifi-scan",.method=HTTP_GET,.handler=wifi_scan_result_json};
    const httpd_uri_t provision_uri={.uri="/api/wifi-provision",.method=HTTP_POST,.handler=command};
    const httpd_uri_t album_create_uri={.uri="/api/album-create",.method=HTTP_POST,.handler=command};
    const httpd_uri_t album_delete_uri={.uri="/api/album-delete",.method=HTTP_POST,.handler=command};
    const httpd_uri_t wifi_settings={.uri="/api/wifi-settings",.method=HTTP_POST,.handler=command};
    const httpd_uri_t album_rename_uri={.uri="/api/album-rename",.method=HTTP_POST,.handler=command};
    err=httpd_register_uri_handler(server,&index);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&album_ui);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&css);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&js);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&status);
#ifdef CONFIG_EPAPER_DIAGNOSTICS
    const httpd_uri_t diagnostic_get={.uri="/api/diagnostics",.method=HTTP_GET,.handler=diagnostics};
    const httpd_uri_t diagnostic_reset={.uri="/api/diagnostics/reset",.method=HTTP_POST,.handler=diagnostics};
    const httpd_uri_t diagnostic_fault={.uri="/api/diagnostics/network-fault",.method=HTTP_POST,.handler=diagnostics};
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&diagnostic_get);
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&diagnostic_reset);
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&diagnostic_fault);
#endif
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&next);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&settings);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&rescan);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&time_sync);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&time_settings);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&wifi_settings);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&wifi_scan);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&wifi_scan_result_uri);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&setup);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&provision_uri);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&album_create_uri);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&album_delete_uri);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&album_rename_uri);
    if(err==ESP_OK) err=httpd_register_uri_handler(server,&captive_uri);
    if(err==ESP_OK) err=httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,captive_redirect);

    const httpd_uri_t announcement_uri={.uri="/api/announcement-settings",.method=HTTP_POST,.handler=command},announcement_pause_uri={.uri="/api/announcement-pause-settings",.method=HTTP_POST,.handler=command},announcement_test_uri={.uri="/api/announcement-test",.method=HTTP_POST,.handler=command};
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&announcement_uri);
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&announcement_pause_uri);
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&announcement_test_uri);
    const httpd_uri_t announcement_asset={.uri="/announcements.js",.method=HTTP_GET,.handler=ui_asset};
    if(err==ESP_OK)err=httpd_register_uri_handler(server,&announcement_asset);
    if(err==ESP_OK) err=epaper_http_register_files(server);
    if(err!=ESP_OK) {epaper_http_stop();return err;}
    char ip[32];
    if(epaper_network_ip(ip,sizeof(ip))) ESP_LOGI("http","Status page: http://%s/",ip);
    return ESP_OK;
}
void epaper_http_update(bool usb_host)
{
    last_usb_host=usb_host;
    if(!lock || !epaper_http_sd_try_lock()) return;
    cJSON *root=cJSON_CreateObject();if(!root) {epaper_http_sd_unlock();return;}
    cJSON *device=cJSON_AddObjectToObject(root,"device");
    cJSON_AddStringToObject(device,"model","ESP32-S3 PhotoPainter");
    cJSON_AddStringToObject(device,"version",esp_app_get_description()->version);
    cJSON_AddBoolToObject(device,"deep_wake",esp_reset_reason()==ESP_RST_DEEPSLEEP);
    uint8_t mac[6];char address[32];
    if(esp_read_mac(mac,ESP_MAC_WIFI_STA)==ESP_OK) {
        snprintf(address,sizeof(address),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        cJSON_AddStringToObject(device,"mac",address);
    }
    uint32_t flash=0;
    if(esp_flash_get_size(esp_flash_default_chip,&flash)==ESP_OK) cJSON_AddNumberToObject(device,"flash_bytes",flash);
    cJSON_AddNumberToObject(device,"psram_bytes",esp_psram_get_size());
    /* Hardware SRAM capacities are fixed for the ESP32-S3 target; heap sizes
       exclude memory reserved for code, static data and other system use. */
    cJSON_AddNumberToObject(device,"internal_sram_bytes",512 * 1024);
    cJSON_AddNumberToObject(device,"rtc_sram_bytes",16 * 1024);
    multi_heap_info_t internal={0},external={0};
    const uint32_t internal_caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    const uint32_t external_caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    heap_caps_get_info(&internal,internal_caps);
    heap_caps_get_info(&external,external_caps);
    cJSON_AddNumberToObject(device,"internal_heap_bytes",heap_caps_get_total_size(internal_caps));
    cJSON_AddNumberToObject(device,"free_internal_bytes",internal.total_free_bytes);
    cJSON_AddNumberToObject(device,"min_free_internal_bytes",internal.minimum_free_bytes);
    cJSON_AddNumberToObject(device,"largest_free_internal_bytes",internal.largest_free_block);
    cJSON_AddNumberToObject(device,"psram_heap_bytes",heap_caps_get_total_size(external_caps));
    cJSON_AddNumberToObject(device,"free_psram_bytes",external.total_free_bytes);
    cJSON_AddNumberToObject(device,"min_free_psram_bytes",external.minimum_free_bytes);
    cJSON_AddNumberToObject(device,"largest_free_psram_bytes",external.largest_free_block);
    cJSON_AddNumberToObject(device,"uptime_seconds",esp_timer_get_time()/1000000);
    cJSON_AddStringToObject(device,"display","800 x 480 / 6 colors");
    cJSON *network=cJSON_AddObjectToObject(root,"network");char ip[32];
    if(epaper_network_ip(ip,sizeof(ip))) cJSON_AddStringToObject(network,"ip",ip);
    cJSON_AddBoolToObject(network,"provisioning",epaper_network_provisioning());
    cJSON_AddNumberToObject(network,"ap_shutdown_remaining_seconds",epaper_network_provision_remaining_seconds());
    cJSON_AddStringToObject(network,"ap_ssid",epaper_network_ap_ssid());
    cJSON_AddStringToObject(network,"ap_ip",epaper_network_provisioning()?EPAPER_PROVISION_IP:"");
    cJSON_AddBoolToObject(network,"usb_host",usb_host);
    cJSON_AddBoolToObject(network,"wifi_connected",epaper_network_online());
    cJSON_AddNumberToObject(network,"next_sync_seconds",epaper_time_sleep_us()/1000000);
    const epaper_time_config_t *time_config=epaper_album_time_config();
    epaper_time_status_t time_status;epaper_time_status(&time_status);
    cJSON_AddBoolToObject(network,"sync_enabled",time_status.enabled);
    cJSON *servers=cJSON_AddArrayToObject(network,"ntp_servers");
    if(time_config) {
        cJSON_AddStringToObject(network,"configured_ssid",time_config->ssid);
        cJSON_AddBoolToObject(network,"wifi_password_set",time_config->password[0]!=0);
        cJSON_AddNumberToObject(network,"sync_interval_seconds",time_config->interval);
        cJSON_AddNumberToObject(network,"retry_seconds",time_config->retry);
        cJSON_AddNumberToObject(network,"sync_timeout_seconds",time_config->sync_timeout);
        for(unsigned i=0;i<time_config->server_count;i++) cJSON_AddItemToArray(servers,cJSON_CreateString(time_config->servers[i]));
    }
    cJSON_AddBoolToObject(network,"sync_attempted",time_status.attempted);
    cJSON_AddStringToObject(network,"sync_result",time_status.attempted?esp_err_to_name(time_status.result):"not-attempted");
    cJSON_AddNumberToObject(network,"last_sync_epoch",time_status.last_success);
    cJSON_AddNumberToObject(network,"last_attempt_epoch",time_status.last_attempt);
    cJSON_AddStringToObject(network,"last_sync_server",time_status.last_server);
    wifi_ap_record_t ap;
    if(epaper_network_online() && esp_wifi_sta_get_ap_info(&ap)==ESP_OK) {
        char ssid[33];memcpy(ssid,ap.ssid,32);ssid[32]=0;
        cJSON_AddStringToObject(network,"ssid",ssid);
        cJSON_AddNumberToObject(network,"rssi_dbm",ap.rssi);
        cJSON_AddNumberToObject(network,"channel",ap.primary);
    }
    epaper_announcement_json(root);
    epaper_album_status_t album;epaper_album_status(&album);
    cJSON *a=cJSON_AddObjectToObject(root,"album");
    cJSON_AddBoolToObject(a,"ready",album.ready);cJSON_AddBoolToObject(a,"enabled",album.enabled);
    cJSON_AddBoolToObject(a,"automatic_stopped",album.automatic_stopped);
    cJSON_AddBoolToObject(a,"night_paused",album.night_paused);
    cJSON_AddNumberToObject(a,"albums",album.albums);cJSON_AddNumberToObject(a,"photos",album.photos);
    cJSON_AddStringToObject(a,"current_album",album.current_album);cJSON_AddStringToObject(a,"current_photo",album.current_photo);
    cJSON_AddNumberToObject(a,"interval_seconds",album.interval);cJSON_AddNumberToObject(a,"next_seconds",album.next_us/1000000);
    const epaper_album_config_t *config=epaper_album_configuration();
    cJSON *night=cJSON_AddObjectToObject(a,"night_pause");
    char night_start[6],night_end[6];
    unsigned start=config?config->night_start:1320,end=config?config->night_end:360;
    snprintf(night_start,sizeof(night_start),"%02u:%02u",(start/60)%24,start%60);
    snprintf(night_end,sizeof(night_end),"%02u:%02u",(end/60)%24,end%60);
    cJSON_AddBoolToObject(night,"enabled",config && config->night_enabled);
    cJSON_AddStringToObject(night,"start",night_start);cJSON_AddStringToObject(night,"end",night_end);
    cJSON *options=cJSON_AddArrayToObject(a,"options");
    if(config) for(unsigned i=0;i<config->album_count;i++) {
        cJSON *option=cJSON_CreateObject();if(!option) continue;
        if(!cJSON_AddItemToArray(options,option)) {cJSON_Delete(option);continue;}
        cJSON_AddStringToObject(option,"id",config->albums[i].id);
        cJSON_AddStringToObject(option,"directory",config->albums[i].directory);
        cJSON_AddBoolToObject(option,"enabled",config->albums[i].enabled);
        cJSON_AddNumberToObject(option,"interval_seconds",config->albums[i].interval);
    }
    cJSON *import=cJSON_AddObjectToObject(root,"image_import");if(import)epaper_image_json(import);
    cJSON *control=cJSON_AddObjectToObject(root,"control");
    cJSON_AddStringToObject(control,"token",token);
    xSemaphoreTake(lock,portMAX_DELAY);
    cJSON_AddNumberToObject(control,"job_id",job_id);
    cJSON_AddStringToObject(control,"state",job_state);
    cJSON_AddStringToObject(control,"error",job_error);
    cJSON_AddStringToObject(control,"kind",announcement_job?"announcement-settings":announcement_pause_job?"announcement-pause-settings":announcement_test_job?"announcement-test":album_create_job?"album-create":album_delete_job?"album-delete":album_rename_job?"album-rename":provision_job?"wifi-provision":wifi_scan_job?"wifi-scan":wifi_job?"wifi-settings":time_settings_job?"time-settings":time_job?"time-sync":settings_job?"settings":rescan_job?"rescan":"next");
    cJSON_AddBoolToObject(control,"busy",pending||running||files_busy||index_pending||index_running);
    cJSON_AddBoolToObject(control,"files_busy",files_busy);
    cJSON_AddBoolToObject(control,"index_pending",index_pending||index_running||(running&&rescan_job));
    cJSON_AddNumberToObject(control,"files_revision",files_revision);
    cJSON_AddStringToObject(control,"index_error",index_error);
    xSemaphoreGive(lock);
    cJSON *storage=cJSON_AddObjectToObject(root,"storage");uint64_t total=0,free_bytes=0;
    bool mounted=esp_vfs_fat_info(EPAPER_SD_MOUNT,&total,&free_bytes)==ESP_OK;
    cJSON_AddBoolToObject(storage,"mounted",mounted);
    if(mounted) {cJSON_AddNumberToObject(storage,"total_bytes",(double)total);cJSON_AddNumberToObject(storage,"free_bytes",(double)free_bytes);}
    epaper_rtc_t rtc;cJSON *clock=cJSON_AddObjectToObject(root,"rtc");
    bool valid=epaper_rtc_read(&rtc)==ESP_OK && rtc.valid;
    int64_t rtc_sample_us=esp_timer_get_time();
    cJSON_AddBoolToObject(clock,"valid",valid);cJSON_AddBoolToObject(clock,"utc",epaper_time_rtc_utc());
    cJSON_AddStringToObject(clock,"chip","PCF85063");
    struct timeval system_time;gettimeofday(&system_time,NULL);
    cJSON_AddNumberToObject(clock,"system_epoch",(double)system_time.tv_sec);
    int64_t external_epoch=0;
    if(valid&&epaper_time_rtc_utc()&&epaper_calendar_epoch_utc(&rtc.calendar,&external_epoch)){cJSON_AddNumberToObject(clock,"rtc_epoch",external_epoch);cJSON_AddNumberToObject(clock,"system_rtc_difference_seconds",system_time.tv_sec-external_epoch);}
    if(valid) {
        const epaper_calendar_t *c=&rtc.calendar;char text[40];
        snprintf(text,sizeof(text),"%04d-%02d-%02dT%02d:%02d:%02d%s",c->year,c->month,c->day,c->hour,c->minute,c->second,epaper_time_rtc_utc()?"Z":"");
        cJSON_AddStringToObject(clock,"calendar",text);
    }
    epaper_environment_t env;cJSON *sensor=cJSON_AddObjectToObject(root,"environment");
    bool measured=epaper_environment_read(&env)==ESP_OK;cJSON_AddBoolToObject(sensor,"valid",measured);
    cJSON_AddStringToObject(sensor,"chip","SHTC3");
    if(measured) {cJSON_AddNumberToObject(sensor,"temperature_c",env.temperature_c);cJSON_AddNumberToObject(sensor,"humidity_percent",env.humidity_percent);}
    epaper_power_t power;cJSON *battery=cJSON_AddObjectToObject(root,"power");
    bool readable=epaper_power_read(&power)==ESP_OK;cJSON_AddBoolToObject(battery,"valid",readable);
    cJSON_AddStringToObject(battery,"chip","AXP2101");
    if(readable) {
        const char *direction[]={"standby","charging","discharging","reserved"};
        const char *phase[]={"trickle","precharge","constant-current","constant-voltage","done","not-charging","reserved","reserved"};
        cJSON_AddBoolToObject(battery,"vbus_good",power.vbus_good);cJSON_AddBoolToObject(battery,"battery_present",power.battery_present);
        cJSON_AddStringToObject(battery,"direction",direction[power.current_direction]);cJSON_AddStringToObject(battery,"phase",phase[power.charge_phase]);
    }
    epaper_http_sd_unlock();
    char *text=cJSON_PrintUnformatted(root);cJSON_Delete(root);if(!text) return;
    xSemaphoreTake(lock,portMAX_DELAY);char *old=snapshot;snapshot=text;snapshot_rtc_sample_us=rtc_sample_us;xSemaphoreGive(lock);free(old);
}

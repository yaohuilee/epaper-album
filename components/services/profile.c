#include "epaper_profile.h"
#include "epaper_network.h"
#include "sdkconfig.h"
#ifdef CONFIG_EPAPER_DIAGNOSTICS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>
#define TASKS 48
typedef struct { UBaseType_t number; char name[configMAX_TASK_NAME_LEN]; uint64_t us[PROFILE_COUNT]; } record_t;
static TaskStatus_t *previous, *current;
static record_t *records;
static unsigned record_count, previous_count, mask;
static unsigned snapshot_failures;
static uint64_t windows[PROFILE_COUNT];
static int64_t previous_at, started_at;
static size_t initial_internal, initial_psram;
static SemaphoreHandle_t mutex;
static TaskHandle_t monitor_task;
static const uint32_t internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT, external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
static const char *names[]={"synthesis","playback","photo","transfer","ntp","fault","import","idle"};
static void sample(void)
{
    uint32_t total;
    unsigned count=uxTaskGetSystemState(current,TASKS,&total);
    int64_t now=esp_timer_get_time();
    if(!count){snapshot_failures++;previous_count=0;previous_at=now;return;}
    unsigned phases=mask?mask:(1u<<PROFILE_IDLE);
    if(previous_count && now>previous_at) {
        for(unsigned phase=0;phase<PROFILE_COUNT;phase++) if(phases&(1u<<phase)) windows[phase]+=(uint64_t)(now-previous_at);
        for(unsigned i=0;i<count;i++) for(unsigned j=0;j<previous_count;j++)
            if(current[i].xTaskNumber==previous[j].xTaskNumber && current[i].xHandle==previous[j].xHandle) {
                /* ESP timer counters wrap at 2^32 us; sample deltas remain short. */
                uint32_t delta=(uint32_t)current[i].ulRunTimeCounter-(uint32_t)previous[j].ulRunTimeCounter;
                unsigned slot=0;
                for(;slot<record_count;slot++) if(records[slot].number==current[i].xTaskNumber)break;
                if(slot==record_count && record_count<TASKS) {
                    records[slot].number=current[i].xTaskNumber;
                    strncpy(records[slot].name,current[i].pcTaskName,sizeof(records[slot].name)-1);record_count++;
                }
                if(slot<TASKS) for(unsigned phase=0;phase<PROFILE_COUNT;phase++)
                    if(phases&(1u<<phase))records[slot].us[phase]+=delta;
                break;
            }
    }
    memcpy(previous,current,count*sizeof(*current));previous_count=count;previous_at=now;
}
static void monitor(void *unused)
{
    (void)unused;
    for(;;) {
        epaper_network_fault_tick();
        xSemaphoreTake(mutex,portMAX_DELAY);sample();xSemaphoreGive(mutex);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
bool epaper_profile_reset(void)
{
    if(!mutex)return false;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(mask){xSemaphoreGive(mutex);return false;}
    heap_caps_monitor_local_minimum_free_size_stop();
    bool ok=heap_caps_monitor_local_minimum_free_size_start()==ESP_OK;
    memset(records,0,TASKS*sizeof(*records));memset(windows,0,sizeof(windows));record_count=previous_count=snapshot_failures=0;
    initial_internal=heap_caps_get_free_size(internal);initial_psram=heap_caps_get_free_size(external);
    started_at=previous_at=esp_timer_get_time();sample();
    xSemaphoreGive(mutex);return ok;
}
void epaper_profile_start(void)
{
    if(mutex)return;
    previous=heap_caps_calloc(TASKS,sizeof(*previous),external);
    current=heap_caps_calloc(TASKS,sizeof(*current),external);
    records=heap_caps_calloc(TASKS,sizeof(*records),external);
    if(!previous||!current||!records)return;
    mutex=xSemaphoreCreateMutex();if(!mutex)return;
    epaper_profile_reset();
    xTaskCreate(monitor,"profile",4096,NULL,2,&monitor_task);
}
bool epaper_profile_available(void){return monitor_task!=NULL;}
void epaper_profile_phase(epaper_profile_phase_t phase,bool active)
{
    if(!mutex||phase>=PROFILE_IDLE)return;
    xSemaphoreTake(mutex,portMAX_DELAY);sample();
    if(active)mask|=1u<<phase;else mask&=~(1u<<phase);
    xSemaphoreGive(mutex);
}
void epaper_profile_json(cJSON *root)
{
    if(!monitor_task){cJSON_AddBoolToObject(root,"available",false);return;}
    xSemaphoreTake(mutex,portMAX_DELAY);sample();
    cJSON_AddBoolToObject(root,"available",true);
    cJSON_AddNumberToObject(root,"cpu_cores",portNUM_PROCESSORS);
    cJSON_AddNumberToObject(root,"snapshot_failures",snapshot_failures);
    cJSON_AddNumberToObject(root,"elapsed_ms",(esp_timer_get_time()-started_at)/1000);
    cJSON_AddNumberToObject(root,"initial_internal_free",initial_internal);
    cJSON_AddNumberToObject(root,"initial_psram_free",initial_psram);
    cJSON_AddNumberToObject(root,"minimum_internal_free",heap_caps_get_minimum_free_size(internal));
    cJSON_AddNumberToObject(root,"minimum_psram_free",heap_caps_get_minimum_free_size(external));
    cJSON_AddNumberToObject(root,"current_internal_free",heap_caps_get_free_size(internal));
    cJSON_AddNumberToObject(root,"current_psram_free",heap_caps_get_free_size(external));
    cJSON_AddNumberToObject(root,"largest_internal_block",heap_caps_get_largest_free_block(internal));
    cJSON_AddNumberToObject(root,"largest_psram_block",heap_caps_get_largest_free_block(external));
    cJSON_AddBoolToObject(root,"network_fault_active",epaper_network_fault_active());
    cJSON *phases=cJSON_AddArrayToObject(root,"phases");
    for(unsigned p=0;p<PROFILE_COUNT;p++)if(windows[p]) {
        cJSON *phase=cJSON_CreateObject();cJSON_AddItemToArray(phases,phase);
        cJSON_AddStringToObject(phase,"name",names[p]);cJSON_AddNumberToObject(phase,"duration_ms",windows[p]/1000);
        cJSON *tasks=cJSON_AddArrayToObject(phase,"tasks");double busy=0;
        for(unsigned r=0;r<record_count;r++)if(records[r].us[p]) {
            double percent=100.0*(double)records[r].us[p]/(double)windows[p]/portNUM_PROCESSORS;
            cJSON *task=cJSON_CreateObject();cJSON_AddItemToArray(tasks,task);
            cJSON_AddStringToObject(task,"name",records[r].name);
            cJSON_AddNumberToObject(task,"cpu_percent_total",percent);
            cJSON_AddNumberToObject(task,"cpu_ms",records[r].us[p]/1000);
            if(strncmp(records[r].name,"IDLE",4))busy+=percent;
        }
        cJSON_AddNumberToObject(phase,"busy_cpu_percent_total",busy);
    }
    xSemaphoreGive(mutex);
}
#else
void epaper_profile_start(void){}
bool epaper_profile_available(void){return false;}
void epaper_profile_phase(epaper_profile_phase_t phase,bool active){(void)phase;(void)active;}
bool epaper_profile_reset(void){return false;}
void epaper_profile_json(cJSON *root){cJSON_AddBoolToObject(root,"available",false);}
#endif

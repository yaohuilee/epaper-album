#include "tts_preview.h"
#include "epaper_announcement_core.h"
#include "epaper_pcm_resample.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef CONFIG_EPAPER_ESP_TTS
#include "esp_tts.h"
#include "esp_tts_voice_template.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static esp_tts_handle_t handle;
static esp_tts_voice_t *voice;
static esp_partition_mmap_handle_t mapping;
static bool mapped;
const char *epaper_tts_voice_name(void)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                                ESP_PARTITION_SUBTYPE_ANY, "voice_data");
    char header[20];
    if (!partition || esp_partition_read(partition, 0, header, sizeof(header)) != ESP_OK)
        return NULL;
    if (!memcmp(header, "xiaole_20220719", 15)) return "xiaole";
    return NULL;
}
bool epaper_tts_available(void) { return epaper_tts_voice_name() != NULL; }
void epaper_tts_close(void)
{
    if (handle) esp_tts_destroy(handle);
    if (voice) esp_tts_voice_set_free(voice);
    if (mapped) esp_partition_munmap(mapping);
    handle = NULL; voice = NULL; mapped = false;
}
static bool open_tts(void)
{
    if (handle) return true;
    if (!epaper_tts_available()) return false;
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                                ESP_PARTITION_SUBTYPE_ANY, "voice_data");
    const void *data = NULL;
    if (!partition || esp_partition_mmap(partition, 0, partition->size,
                                        ESP_PARTITION_MMAP_DATA, &data, &mapping) != ESP_OK)
        return false;
    mapped = true;
    voice = esp_tts_voice_set_init(&esp_tts_voice_template, (void *)data);
    if (voice) handle = esp_tts_create(voice);
    if (!handle) { epaper_tts_close(); return false; }
    return true;
}
bool epaper_tts_synthesize(const char *text, int16_t **pcm, size_t *samples, void *context)
{
    *pcm = NULL; *samples = 0;
    if (!text || !*text || strlen(text) > 256 || !open_tts()) return false;
    esp_tts_stream_reset(handle);
    if (!esp_tts_parse_chinese(handle, text)) return false;
    int16_t *raw = NULL;
    size_t count = 0, capacity = 0;
    bool ok = false;
    int64_t deadline = esp_timer_get_time() + 5000000;
    if (context && *(const int64_t *)context < deadline) deadline = *(const int64_t *)context;
    for (;;) {
        if (esp_timer_get_time() >= deadline) break;
        int length = 0;
        short *chunk = esp_tts_stream_play(handle, &length, 2);
        if (!length) { ok = count > 0; break; }
        if (!chunk || length < 0 || (size_t)length > 16000u * 15u - count) break;
        size_t needed = count + (size_t)length;
        if (needed > capacity) {
            size_t next = (needed + 4095) / 4096 * 4096;
            int16_t *grown = heap_caps_realloc(raw, next * sizeof(*raw), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!grown) break;
            raw = grown; capacity = next;
        }
        memcpy(raw + count, chunk, (size_t)length * sizeof(*raw)); count = needed;
        vTaskDelay(1);
    }
    esp_tts_stream_reset(handle);
    if (ok) {
        size_t total = count * 3 / 2;
        *pcm = heap_caps_malloc(total * sizeof(**pcm), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (*pcm) *samples = epaper_pcm_16_to_24(raw, count, *pcm, total);
        ok = *samples > 0;
    }
    free(raw);
    if (!ok) { free(*pcm); *pcm = NULL; }
    return ok;
}
#else
bool epaper_tts_available(void) { return false; }
const char *epaper_tts_voice_name(void) { return NULL; }
void epaper_tts_close(void) {}
bool epaper_tts_synthesize(const char *text, int16_t **pcm, size_t *samples, void *context)
{
    (void)text; (void)context; *pcm = NULL; *samples = 0; return false;
}
#endif

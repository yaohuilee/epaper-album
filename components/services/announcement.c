#include "epaper_announcement.h"
#include "epaper_announcement_core.h"
#include "epaper_audio_output.h"
#include "epaper_audio_sequence.h"
#include "tts_preview.h"
#include "epaper_profile.h"
#include "epaper_devices.h"
#include "epaper_http.h"
#include "epaper_network.h"
#include "epaper_runtime.h"
#include "epaper_storage.h"
#include "epaper_time_sync.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
static SemaphoreHandle_t mutex;
static nvs_handle_t dedup_store;
static bool store_open, ready, active, test_pending, test_half, sleeping, config_valid;
static bool test_active;
static unsigned test_id, completed_test_id;
static const char *test_result = "none";
static char test_error[160];
static unsigned test_volume;
static const char *last_test_engine = "esp-tts";
static unsigned test_synthesis_ms;
static epaper_announcement_config_t config;
static int64_t target, processed, last_target, sequence_start, sequence_length;
static int64_t sequence_mono_start, prefix_offset, time_offset, environment_offset;
static const char *phase = "disabled", *last_result = "none";
static char error_text[160];
static epaper_environment_t sample;
static bool sample_valid;
static int64_t sample_epoch;
static int64_t wall_us(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (int64_t)t.tv_sec * 1000000 + t.tv_usec;
}
static void take(void) { xSemaphoreTake(mutex, portMAX_DELAY); }
static void give(void) { xSemaphoreGive(mutex); }
static bool rtc_epoch_near_system(const epaper_rtc_t *rtc, int64_t *epoch)
{
    time_t now = (time_t)(wall_us() / 1000000);
    const epaper_calendar_t *c = &rtc->calendar;
    for (int offset = -2; offset <= 2; offset++)
    {
        time_t candidate = now + offset;
        struct tm t;
        if (gmtime_r(&candidate, &t) && c->year == t.tm_year + 1900 && c->month == t.tm_mon + 1 &&
            c->day == t.tm_mday && c->hour == t.tm_hour && c->minute == t.tm_min &&
            c->second == t.tm_sec)
        {
            if (epoch)
                *epoch = candidate;
            return true;
        }
    }
    return false;
}
/* Observe a real RTC second transition instead of assuming the RTC and the
   system clock have identical subsecond phase. Bus reads are serialized by
   the board layer. The 10 ms polling resolution is not an acoustic guarantee. */
static bool rtc_countdown_anchor(int64_t target_epoch, bool half, int64_t *anchor)
{
    int64_t deadline = esp_timer_get_time() + 1300000, previous = -1;
    while (esp_timer_get_time() < deadline)
    {
        epaper_rtc_t rtc;
        int64_t epoch = 0;
        int64_t before = esp_timer_get_time();
        if (epaper_rtc_read(&rtc) != ESP_OK || !rtc.valid || !rtc_epoch_near_system(&rtc, &epoch))
            return false;
        int64_t observed = (before + esp_timer_get_time()) / 2;
        if (previous >= 0 && epoch == previous + 1)
        {
            *anchor = observed + (target_epoch - epoch - (half ? 2 : 4)) * 1000000;
            return true;
        }
        if (previous >= 0 && epoch != previous)
            return false;
        previous = epoch;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}
bool epaper_announcement_prepare_sleep(void)
{
    if (!mutex)
        return true;
    take();
    bool allowed =
        !(active || test_pending) && (!ready || !config.enabled || !config.volume || !target ||
                                      (target - 20) * 1000000 - wall_us() > 250000);
    if (allowed)
        sleeping = true;
    give();
    return allowed;
}
static bool refresh_config(char *error, size_t size)
{
    char *global = NULL;
    epaper_announcement_config_t next;
    if (!epaper_album_root_json(&global))
    {
        snprintf(error, size, "SD root configuration unavailable");
        return false;
    }
    bool ok = epaper_announcement_parse(global, &next, error, size);
    char *migrated = NULL;
    if (ok) {
        ok = epaper_announcement_migrate(global, &migrated, error, size);
        if (ok && migrated) ok = epaper_album_save_root_json(migrated, error, size) == ESP_OK;
    }
    free(migrated);
    free(global);
    if (ok)
    {
        bool loaded = epaper_tts_available();
        take();
        config = next;
        ready = loaded;
        config_valid = true;
        target = loaded ? epaper_announcement_next(&next, wall_us() / 1000000 + 6, processed) : 0;
        give();
    }
    return ok;
}
bool epaper_announcement_busy(void)
{
    if (!mutex)
        return false;
    take();
    bool value = active || test_pending;
    give();
    return value;
}
bool epaper_announcement_admit(unsigned seconds)
{
    if (!mutex)
        return true;
    take();
    int64_t next = target;
    bool playing = active || test_pending, enabled = ready && config.enabled && config.volume;
    give();
    return !playing &&
           (!enabled || !next || next * 1000000 - wall_us() > (int64_t)(seconds + 20) * 1000000);
}
uint64_t epaper_announcement_sleep_us(void)
{
    if (!mutex)
        return 0;
    take();
    int64_t next = target;
    bool enabled = config.enabled && config.volume && ready;
    give();
    if (!enabled || !next)
        return 0;
    return epaper_announcement_wake_delay(next, wall_us());
}
static void finish(const char *result, const char *error)
{
    epaper_audio_output_close();
    take();
    active = false;
    target = 0;
    phase = "idle";
    last_result = result;
    if (test_active)
    {
        completed_test_id = test_id;
        test_result = result;
        snprintf(test_error, sizeof(test_error), "%s", error ? error : "");
        test_active = false;
    }
    snprintf(error_text, sizeof(error_text), "%s", error ? error : "");
    give();
}
static void worker(void *unused)
{
    (void)unused;
    int64_t previous_wall = 0, previous_mono = 0;
    for (;;)
    {
        int64_t wall = wall_us(), mono = esp_timer_get_time(), now = wall / 1000000;
        bool portal = epaper_network_provisioning();
        take();
        if (sleeping)
        {
            give();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (previous_wall && llabs((wall - previous_wall) - (mono - previous_mono)) > 250000)
            target = 0;
        previous_wall = wall;
        previous_mono = mono;
        epaper_announcement_config_t selected = config;
        bool assets = ready, testing = test_pending;
        unsigned volume = testing ? test_volume : selected.volume;
        bool half = testing ? test_half : false;
        if (testing)
        {
            last_test_engine = "esp-tts";
            test_synthesis_ms = 0;
            test_pending = false;
            test_active = true;
            active = true;
            phase = "preparing";
        }
        if (!testing)
        {
            if (!config_valid || !selected.enabled || !selected.volume || !assets || portal ||
                now < 1577836800)
            {
                target = 0;
                phase = !config_valid       ? "config-invalid"
                        : portal            ? "provisioning"
                        : !selected.enabled ? "disabled"
                        : !selected.volume  ? "muted"
                        : !assets           ? "resources-missing"
                                            : "time-invalid";
                give();
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
            if (!target || target < now || target > now + 90000)
                target = epaper_announcement_next(&selected, now + 6, processed);
            if (!target)
            {
                phase = "all-day-paused";
                give();
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
            if (now < target - 20)
            {
                phase = epaper_announcement_paused(&selected, now, NULL) ? "paused" : "idle";
                give();
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            active = true;
            phase = "preparing";
            half = (target / 60 % 60) == 30;
        }
        int64_t this_target = testing ? now : target;
        last_target = this_target;
        give();
        epaper_rtc_t rtc;
        epaper_audio_sequence_t sequence = {0};
        epaper_environment_t environment = {0};
        bool rtc_valid = epaper_rtc_read(&rtc) == ESP_OK && rtc.valid;
        if (!testing &&
            (!rtc_valid || !epaper_time_rtc_utc() || !rtc_epoch_near_system(&rtc, NULL)))
        {
            finish("skipped", "RTC is invalid or differs from system time");
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (portal)
        {
            finish("skipped", "Provisioning mode is active");
            continue;
        }
        int64_t sd_timeout = esp_timer_get_time() + 2000000;
        bool sd_acquired = false;
        do
        {
            sd_acquired = epaper_http_sd_try_lock();
            if (!sd_acquired)
                vTaskDelay(pdMS_TO_TICKS(10));
        } while (!sd_acquired && esp_timer_get_time() < sd_timeout);
        if (!sd_acquired)
        {
            finish("skipped", "SD is busy; countdown was not started");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        esp_err_t environment_error = epaper_environment_read(&environment);
        int64_t measurement_epoch = wall_us() / 1000000;
        unsigned hour = (unsigned)((this_target + 28800) / 3600 % 24);
        if (testing)
            hour = rtc_valid
                       ? (unsigned)((rtc.calendar.hour + (epaper_time_rtc_utc() ? 8 : 0)) % 24)
                       : 10;
        epaper_speech_synthesizer_t loader = epaper_tts_synthesize;
        int64_t synthesis_start = esp_timer_get_time();
        epaper_profile_phase(PROFILE_SYNTHESIS,true);
        int64_t synthesis_deadline = synthesis_start + 10000000;
        bool built = epaper_audio_sequence_build(
            &sequence, hour, half, &selected, environment_error == ESP_OK,
            environment.temperature_c, environment.humidity_percent, loader,
            &synthesis_deadline);
        bool time_only = false;
        if (!built && (selected.temperature || selected.humidity))
        {
            epaper_announcement_config_t without_environment = selected;
            without_environment.temperature = without_environment.humidity = false;
            built = epaper_audio_sequence_build(&sequence, hour, half, &without_environment, false,
                                                0, 0, loader, &synthesis_deadline);
            time_only = built;
        }
        int64_t expected_first =
            testing ? wall_us() + 500000 : this_target * 1000000 - (half ? 2 : 4) * 1000000;
        if (built && sequence.environment_start &&
            expected_first + (int64_t)sequence.environment_start * 1000000 / ANNOUNCEMENT_RATE -
                    measurement_epoch * 1000000 >
                30000000)
        {
            epaper_audio_sequence_free(&sequence);
            built = epaper_audio_sequence_build(&sequence, hour, half, &selected, false, 0, 0,
                                                loader, &synthesis_deadline);
            environment_error = ESP_ERR_TIMEOUT;
        }
        epaper_tts_close();
        epaper_profile_phase(PROFILE_SYNTHESIS,false);
        {
            take();
            test_synthesis_ms = (unsigned)((esp_timer_get_time() - synthesis_start) / 1000);
            give();
        }
        epaper_http_sd_unlock();
        if (!built)
        {
            finish("error", "ESP-TTS synthesis failed or insufficient memory");
            continue;
        }
        int64_t first = this_target * 1000000 - (half ? 2 : 4) * 1000000;
        if (testing)
            first = wall_us() + 500000;
        int64_t delta = first - wall_us();
        if (!testing && !epaper_announcement_countdown_window(delta))
        {
            epaper_audio_sequence_free(&sequence);
            take();
            if (this_target > processed)
                processed = this_target;
            give();
            finish("skipped", "Countdown deadline missed or clock changed during preparation");
            continue;
        }
        if (!testing)
        {
            if (!store_open || nvs_set_i64(dedup_store, "last_slot", this_target) != ESP_OK ||
                nvs_commit(dedup_store) != ESP_OK)
            {
                epaper_audio_sequence_free(&sequence);
                finish("error", "Cannot persist announcement deduplication marker");
                continue;
            }
            take();
            if (this_target > processed)
                processed = this_target;
            give();
        }
        esp_err_t output = epaper_audio_output_prepare();
        if (output != ESP_OK)
        {
            epaper_audio_sequence_free(&sequence);
            finish("error", esp_err_to_name(output));
            continue;
        }
        int64_t anchor = esp_timer_get_time() + (testing ? 500000 : first - wall_us());
        int64_t tone_target = testing ? wall_us() / 1000000 + (half ? 2 : 4) + 2 : this_target;
        if (!testing && !rtc_countdown_anchor(tone_target, half, &anchor))
        {
            epaper_audio_sequence_free(&sequence);
            finish("skipped", "RTC second transition was not verified");
            continue;
        }
        if (anchor < esp_timer_get_time() + 10000)
        {
            epaper_audio_sequence_free(&sequence);
            finish("skipped", "Audio setup missed countdown start");
            continue;
        }
        first = wall_us() + anchor - esp_timer_get_time();
        if (environment_error == ESP_OK && sequence.environment_start &&
            first + (int64_t)sequence.environment_start * 1000000 / ANNOUNCEMENT_RATE -
                    measurement_epoch * 1000000 >
                30000000)
        {
            sequence.samples = sequence.time_end + ANNOUNCEMENT_RATE / 10;
            time_only = true;
            environment_error = ESP_ERR_TIMEOUT;
        }
        take();
        sample = environment;
        sample_valid = environment_error == ESP_OK;
        sample_epoch = measurement_epoch;
        sequence_start = first;
        sequence_mono_start = anchor;
        prefix_offset = (int64_t)sequence.prefix_start * 1000000 / ANNOUNCEMENT_RATE;
        time_offset = (int64_t)sequence.time_start * 1000000 / ANNOUNCEMENT_RATE;
        environment_offset = (int64_t)sequence.environment_start * 1000000 / ANNOUNCEMENT_RATE;
        sequence_length = (int64_t)sequence.samples * 1000000 / ANNOUNCEMENT_RATE;
        phase = testing ? "testing" : "armed";
        give();
        ESP_LOGI("announcement", "%s target=%lld beeps=%u samples=%u first_in_ms=%lld",
                 testing ? "TEST" : "AUTO", (long long)this_target, sequence.beep_count,
                 (unsigned)sequence.samples, (long long)((first - wall_us()) / 1000));
        epaper_profile_phase(PROFILE_PLAYBACK,true);
        output = epaper_audio_output_play(sequence.pcm, sequence.samples, volume, anchor);
        epaper_profile_phase(PROFILE_PLAYBACK,false);
        epaper_audio_sequence_free(&sequence);
        finish(output == ESP_OK ? (time_only ? "partial" : "succeeded") : "error",
               output == ESP_OK
                   ? (time_only ? "Environment voice unavailable; completed time only" : NULL)
                   : esp_err_to_name(output));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
esp_err_t epaper_announcement_start(void)
{
    if (mutex)
        return ESP_ERR_INVALID_STATE;
    mutex = xSemaphoreCreateMutex();
    if (!mutex)
        return ESP_ERR_NO_MEM;
    if (epaper_http_sd_try_lock())
        epaper_http_sd_unlock();
    if (nvs_open_from_partition("album_state", "announcement", NVS_READWRITE, &dedup_store) ==
        ESP_OK)
    {
        store_open = true;
        nvs_get_i64(dedup_store, "last_slot", &processed);
    }
    if (!refresh_config(error_text, sizeof(error_text)))
    {
        phase = "config-invalid";
        ready = false;
        config.enabled = false;
    }
    if (xTaskCreate(worker, "announcement", 16384, NULL, 6, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}
esp_err_t epaper_announcement_settings(const char *request, bool periods, char *error, size_t size)
{
    if (!mutex || epaper_announcement_busy())
    {
        snprintf(error, size, "Announcement is busy");
        return ESP_ERR_INVALID_STATE;
    }
    char *global = NULL, *output = NULL;
    epaper_announcement_config_t next;
    if (!epaper_album_root_json(&global))
        return ESP_FAIL;
    bool edited = epaper_announcement_edit(global, request, periods, &output, &next, error, size);
    free(global);
    if (!edited)
        return ESP_ERR_INVALID_ARG;
    if (next.enabled && !epaper_tts_available())
    {
        free(output);
        snprintf(error, size,
                 "Install the Xiaole Flash voice image before enabling announcements");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = epaper_album_save_root_json(output, error, size);
    free(output);
    if (err == ESP_OK)
    {
        bool loaded = epaper_tts_available();
        take();
        config = next;
        ready = loaded;
        config_valid = true;
        target = 0;
        give();
    }
    return err;
}
esp_err_t epaper_announcement_test(const char *request, char *error, size_t size)
{
    if (!mutex)
        return ESP_ERR_INVALID_STATE;
    cJSON *v = epaper_announcement_json_parse(request, 16384);
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(v, "kind"),
                *volume = cJSON_GetObjectItemCaseSensitive(v, "volume_percent");
    const cJSON *engine = cJSON_GetObjectItemCaseSensitive(v, "engine");
    bool use_tts = !engine || (cJSON_IsString(engine) && !strcmp(engine->valuestring, "esp-tts"));
    bool ok = cJSON_IsObject(v) && cJSON_IsString(kind) &&
              (!strcmp(kind->valuestring, "hour") || !strcmp(kind->valuestring, "half")) &&
              cJSON_IsNumber(volume) && volume->valuedouble >= 1 && volume->valuedouble <= 100 &&
              volume->valuedouble == (double)(unsigned)volume->valuedouble &&
              (!engine || (cJSON_IsString(engine) &&
                           use_tts));
    unsigned count = 0;
    for (const cJSON *f = v ? v->child : NULL; f; f = f->next)
    {
        count++;
        if (!f->string || (strcmp(f->string, "kind") && strcmp(f->string, "volume_percent") && strcmp(f->string, "engine")))
            ok = false;
    }
    if (count != (engine ? 3u : 2u))
        ok = false;
    if (!ok)
    {
        cJSON_Delete(v);
        snprintf(error, size, "Use kind hour/half and volume_percent 1..100");
        return ESP_ERR_INVALID_ARG;
    }
    if (!epaper_announcement_admit(55))
    {
        cJSON_Delete(v);
        snprintf(error, size, "Automatic announcement is preparing or imminent");
        return ESP_ERR_INVALID_STATE;
    }
    if (use_tts && !epaper_tts_available()) {
        cJSON_Delete(v);
        snprintf(error, size, "Xiaole Flash voice image unavailable");
        return ESP_ERR_NOT_SUPPORTED;
    }
    take();
    if (!ready || active || test_pending)
    {
        give();
        cJSON_Delete(v);
        snprintf(error, size, "Speech synthesis unavailable or playback busy");
        return ESP_ERR_INVALID_STATE;
    }
    test_half = !strcmp(kind->valuestring, "half");
    test_volume = (unsigned)volume->valuedouble;
    test_pending = true;
    test_id++;
    give();
    cJSON_Delete(v);
    return ESP_OK;
}
void epaper_announcement_json(cJSON *root)
{
    cJSON *a = cJSON_AddObjectToObject(root, "announcement");
    if (!a)
        return;
    epaper_announcement_config_t c = {0};
    epaper_environment_t env = {0};
    int64_t next = 0, last = 0, measured = 0, recorded = 0;
    unsigned requested_test = 0, completed_test = 0;
    const char *completed_result = "none";
    const char *completed_engine = "esp-tts";
    unsigned synthesis_ms = 0;
    char completed_error[160] = {0};
    bool loaded = false, playing = false, valid = false;
    char error[160] = {0};
    const char *state = "unavailable", *result = "none";
    if (mutex)
    {
        take();
        c = config;
        next = target;
        last = last_target;
        recorded = processed;
        requested_test = test_id;
        completed_test = completed_test_id;
        completed_result = test_result;
        completed_engine = last_test_engine;
        synthesis_ms = test_synthesis_ms;
        snprintf(completed_error, sizeof(completed_error), "%s", test_error);
        loaded = ready;
        playing = active || test_pending;
        state = phase;
        result = last_result;
        env = sample;
        valid = sample_valid;
        measured = sample_epoch;
        snprintf(error, sizeof(error), "%s", error_text);
        if (active && (!strcmp(phase, "armed") || !strcmp(phase, "testing")))
        {
            int64_t elapsed = esp_timer_get_time() - sequence_mono_start;
            state = elapsed < 0                                           ? "armed"
                    : environment_offset && elapsed >= environment_offset ? "environment-phrase"
                    : elapsed >= time_offset                              ? "time-phrase"
                    : elapsed >= prefix_offset                            ? "prefix"
                                                                          : "beeping";
        }
        give();
    }
    cJSON_AddBoolToObject(a, "ready", loaded);
    cJSON_AddBoolToObject(a, "tts_available", epaper_tts_available());
    const char *tts_voice = epaper_tts_voice_name();
    cJSON_AddStringToObject(a, "tts_voice", tts_voice ? tts_voice : "");
    cJSON_AddBoolToObject(a, "tts_test_available", !playing && !epaper_network_provisioning() && epaper_announcement_admit(150));
    cJSON_AddStringToObject(a, "last_test_engine", completed_engine);
    cJSON_AddNumberToObject(a, "test_synthesis_ms", synthesis_ms);
    cJSON_AddBoolToObject(a, "enabled", c.enabled);
    cJSON_AddBoolToObject(a, "busy", playing);
    cJSON_AddBoolToObject(a,"test_available",loaded&&!playing&&!epaper_network_provisioning()&&(!c.enabled||!c.volume||!next||next*1000000-wall_us()>170000000));
    cJSON_AddStringToObject(a, "phase", state);
    cJSON_AddStringToObject(a, "last_result", result);
    cJSON_AddStringToObject(a, "error", error);
    cJSON_AddStringToObject(a, "engine", "esp-tts");
    cJSON_AddStringToObject(a, "voice", "xiaole");
    cJSON_AddNumberToObject(a, "speech_speed", 2);
    cJSON_AddNumberToObject(a, "volume_percent", c.volume);
    cJSON_AddBoolToObject(a, "hourly", c.hourly);
    cJSON_AddBoolToObject(a, "half_hourly", c.half_hourly);
    cJSON_AddNumberToObject(a, "next_target_epoch", next);
    cJSON_AddNumberToObject(a, "next_seconds",
                            next > wall_us() / 1000000 ? next - wall_us() / 1000000 : 0);
    cJSON_AddNumberToObject(a, "last_target_epoch", last);
    cJSON_AddNumberToObject(a, "last_processed_epoch", recorded);
    cJSON_AddBoolToObject(a, "dedup_storage_ready", store_open);
    cJSON_AddNumberToObject(a, "test_request_id", requested_test);
    cJSON_AddNumberToObject(a, "completed_test_id", completed_test);
    cJSON_AddStringToObject(a, "last_test_result", completed_result);
    cJSON_AddStringToObject(a, "last_test_error", completed_error);
    cJSON_AddNumberToObject(a, "last_sample_epoch", measured);
    cJSON_AddBoolToObject(a, "environment_valid", valid);
    if (valid)
    {
        cJSON_AddNumberToObject(a, "temperature_c", env.temperature_c);
        cJSON_AddNumberToObject(a, "humidity_percent", env.humidity_percent);
    }
    cJSON *periods = cJSON_AddArrayToObject(a, "pause_periods");
    for (unsigned i = 0; i < c.period_count; i++)
    {
        const epaper_pause_period_t *p = &c.periods[i];
        cJSON *v = cJSON_CreateObject();
        char start[6], end[6];
        snprintf(start, 6, "%02u:%02u", p->start / 60 % 24, p->start % 60);
        snprintf(end, 6, "%02u:%02u", p->end / 60 % 24, p->end % 60);
        cJSON_AddStringToObject(v, "id", p->id);
        cJSON_AddStringToObject(v, "name", p->name);
        cJSON_AddBoolToObject(v, "enabled", p->enabled);
        cJSON_AddStringToObject(v, "start", start);
        cJSON_AddStringToObject(v, "end", end);
        cJSON_AddItemToArray(periods, v);
    }
    int period = -1;
    cJSON_AddBoolToObject(a, "paused",
                          epaper_announcement_paused(&c, wall_us() / 1000000, &period));
    cJSON_AddStringToObject(a, "pause_name", period >= 0 ? c.periods[period].name : "");
}

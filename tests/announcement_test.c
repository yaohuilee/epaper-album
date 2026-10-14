#include "epaper_announcement_core.h"
#include "epaper_audio_sequence.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static unsigned allocation_count, allocation_failure;
static void *fault_malloc(size_t size)
{
    if (++allocation_count == allocation_failure) return NULL;
    return malloc(size);
}
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        checks++;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "FAIL announcement line %d: %s\n", __LINE__, #x);                      \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static char played[2048];
static bool load(const char *name, int16_t **pcm, size_t *count, void *context)
{
    if (context && !strcmp(name, (const char *)context))
        return false;
    CHECK(name && *name);
    strcat(played, name);
    strcat(played, ";");
    *count = 2400;
    *pcm = malloc(*count * 2);
    CHECK(*pcm);
    for (size_t i = 0; i < *count; i++)
        (*pcm)[i] = 123;
    return true;
}
int main(void)
{
    /* Fast synthesis after the new T-20 preparation window must not be skipped,
       especially half-hour reports with environment speech disabled. */
    CHECK(epaper_announcement_countdown_window(16000000 - 500000));
    CHECK(epaper_announcement_countdown_window(18000000 - 500000));
    CHECK(epaper_announcement_countdown_window(100000));
    CHECK(epaper_announcement_countdown_window(25000000));
    CHECK(!epaper_announcement_countdown_window(99999));
    CHECK(!epaper_announcement_countdown_window(25000001));
    CHECK(epaper_announcement_wake_delay(0,0)==0);
    CHECK(epaper_announcement_wake_delay(2000,1970LL*1000000)==1000000);
    CHECK(epaper_announcement_wake_delay(2000,1960LL*1000000)==10000000);
    CHECK(epaper_announcement_wake_delay(2000,1800LL*1000000)==60000000);
    CHECK(epaper_announcement_wake_delay(2000,1300LL*1000000)==70000000);
    CHECK(epaper_announcement_wake_delay(2000,0)==900000000);

    epaper_announcement_config_t c;
    char error[160], *output = NULL;
    CHECK(epaper_announcement_parse("{\"schema_version\":1}", &c, error, sizeof(error)));
    CHECK(!c.enabled && c.volume == 40 && c.hourly && c.half_hourly && c.temp_decimals == 1 &&
          c.period_count == 1);
    const char *root = "{\"schema_version\":1,\"album\":{\"root\":\"/"
                       "albums\"},\"network\":{\"ssid\":\"synthetic-test\"},\"announcement\":{"
                       "\"quiet_hours\":{\"enabled\":true,\"start\":\"22:00\",\"end\":\"08:00\"}}}";
    CHECK(epaper_announcement_parse(root, &c, error, sizeof(error)) && c.periods[0].end == 480);
    CHECK(epaper_announcement_edit(
        root, "{\"schema_version\":1,\"enabled\":true,\"minutes\":[0,30],\"volume_percent\":40}",
        false, &output, &c, error, sizeof(error)));
    CHECK(strstr(output, "synthetic-test") && strstr(output, "pause_periods") &&
          !strstr(output, "quiet_hours"));
    free(output);
    c.periods[0].end = 360;
    const int64_t day = 1767225600; /* UTC midnight, device time +8 h. */
    CHECK(epaper_announcement_paused(&c, day + 14 * 3600, NULL));
    CHECK(!epaper_announcement_paused(&c, day + 22 * 3600, NULL));
    CHECK(epaper_announcement_paused(&c, day + 22 * 3600 - 1, NULL));
    CHECK(epaper_announcement_next(&c, day + 14 * 3600, 0) == day + 22 * 3600);
    CHECK(epaper_announcement_next(&c, day + 22 * 3600 - 4, 0) == day + 22 * 3600);
    CHECK(epaper_announcement_next(&c, day + 22 * 3600, day + 22 * 3600) == day + 22 * 3600 + 1800);
    c.hourly = false;
    CHECK(epaper_announcement_next(&c, day + 22 * 3600, 0) == day + 22 * 3600 + 1800);
    c.hourly = true;
    c.volume = 0;
    CHECK(!epaper_announcement_next(&c, day, 0));
    c.volume = 40;
    CHECK(!epaper_announcement_next(&c, 1000, 0));
    const char *multi = "{\"schema_version\":1,\"pause_periods\":[{\"id\":\"one\",\"name\":"
                        "\"night\",\"enabled\":true,\"start\":\"22:00\",\"end\":\"06:00\"},{\"id\":"
                        "\"two\",\"start\":\"05:00\",\"end\":\"09:00\"}]}";
    CHECK(epaper_announcement_edit(root, multi, true, &output, &c, error, sizeof(error)));
    free(output);
    c.enabled = true;
    CHECK(epaper_announcement_next(&c, day + 14 * 3600, 0) == day + 25 * 3600);
    c.periods[0].start = 0;
    c.periods[0].end = 720;
    c.periods[1].start = 720;
    c.periods[1].end = 0;
    CHECK(!epaper_announcement_next(&c, day, 0));
    CHECK(!epaper_announcement_edit(root,
                                    "{\"schema_version\":1,\"pause_periods\":[{\"id\":\"same\","
                                    "\"start\":\"22:00\",\"end\":\"22:00\"}]}",
                                    true, &output, &c, error, sizeof(error)));
    CHECK(!epaper_announcement_edit(root,
                                    "{\"schema_version\":1,\"pause_periods\":[{\"id\":\"same\","
                                    "\"start\":\"24:00\",\"end\":\"06:00\"}]}",
                                    true, &output, &c, error, sizeof(error)));
    CHECK(!epaper_announcement_edit(
        root, "{\"schema_version\":1,\"enabled\":true,\"minutes\":[0,0],\"volume_percent\":40}",
        false, &output, &c, error, sizeof(error)));
    CHECK(!epaper_announcement_edit(
        root, "{\"schema_version\":1,\"enabled\":true,\"minutes\":[0,15],\"volume_percent\":40}",
        false, &output, &c, error, sizeof(error)));
    CHECK(!epaper_announcement_edit(
        root, "{\"schema_version\":1,\"enabled\":true,\"minutes\":[0],\"volume_percent\":101}",
        false, &output, &c, error, sizeof(error)));
    CHECK(!epaper_announcement_parse(
        "{\"schema_version\":1,\"announcement\":{\"enabled\":true,\"enabled\":false}}", &c, error,
        sizeof(error)));
    CHECK(epaper_announcement_parse("{\"schema_version\":1}", &c, error, sizeof(error)));
    const char *legacy = "{\"schema_version\":1,\"network\":{\"ssid\":\"test-only\"},\"announcement\":{\"enabled\":true,\"volume_percent\":55,\"voice_pack\":\"old-pack\",\"audio_root\":\"/audio/zh-CN\"}}";
    CHECK(epaper_announcement_migrate(legacy, &output, error, sizeof(error)));
    CHECK(output && !strstr(output, "voice_pack") && !strstr(output, "audio_root"));
    CHECK(strstr(output, "xiaole") && strstr(output, "esp-tts") && strstr(output, "test-only"));
    CHECK(epaper_announcement_parse(output, &c, error, sizeof(error)) && c.enabled && c.volume == 55);
    char *again = NULL;
    CHECK(epaper_announcement_migrate(output, &again, error, sizeof(error)) && !again);
    free(output);
    CHECK(!epaper_announcement_migrate("{broken", &output, error, sizeof(error)) && !output);
    for (unsigned fault = 1; fault <= 150; fault++) {
        cJSON_Hooks hooks = {fault_malloc, free};
        allocation_count = 0; allocation_failure = fault;
        cJSON_InitHooks(&hooks);
        bool migrated = epaper_announcement_migrate(legacy, &output, error, sizeof(error));
        cJSON_InitHooks(NULL);
        CHECK(migrated ? output != NULL : output == NULL);
        if (migrated) CHECK(strstr(output, "xiaole") && strstr(output, "test-only"));
        free(output);
    }
    CHECK(epaper_announcement_parse("{\"schema_version\":1}", &c, error, sizeof(error)));
    epaper_audio_sequence_t sequence;
    CHECK(epaper_audio_sequence_build(&sequence, 10, false, &c, true, 26.5f, 61.6f, load, NULL));
    CHECK(sequence.beep_count == 5 && sequence.last_beep_start == 4 * 24000);
    CHECK(sequence.last_beep_end - sequence.last_beep_start == 6000);
    CHECK(sequence.prefix_start - sequence.last_beep_end == 24000);
    CHECK(sequence.time_start - sequence.prefix_end == 24000);
    CHECK(sequence.environment_start - sequence.time_end == 24000);
    for (size_t i = sequence.time_end; i < sequence.environment_start; i++)
        CHECK(sequence.pcm[i] == 0);
    CHECK(strstr(played, "10点整") && strstr(played, "温度26.5摄氏度") &&
          strstr(played, "湿度百分之62"));
    epaper_audio_sequence_free(&sequence);
    played[0] = 0;
    CHECK(epaper_audio_sequence_build(&sequence, 0, true, &c, true, -0.01f, 100, load, NULL));
    CHECK(sequence.last_beep_start == 2 * 24000 && sequence.beep_count == 3);
    CHECK(!strstr(played, "零下") && strstr(played, "0点三十分") &&
          strstr(played, "湿度百分之100"));
    epaper_audio_sequence_free(&sequence);
    played[0] = 0;
    CHECK(epaper_audio_sequence_build(&sequence, 23, false, &c, false, NAN, NAN, load, NULL));
    CHECK(strstr(played, "温湿度暂不可用") && !strstr(played, "温度26.5摄氏度"));
    epaper_audio_sequence_free(&sequence);
    /* A missing environment clip frees the incomplete sequence. Time-only
       fallback still succeeds; a missing time clip cannot produce a report. */
    played[0] = 0;
    CHECK(!epaper_audio_sequence_build(&sequence, 10, false, &c, true, 26.5f, 62,
                                      load, "温度26.5摄氏度"));
    CHECK(!sequence.pcm && !sequence.samples && !sequence.capacity);
    epaper_announcement_config_t time_only = c;
    time_only.temperature = time_only.humidity = false;
    played[0] = 0;
    CHECK(epaper_audio_sequence_build(&sequence, 10, false, &time_only, true, 26.5f, 62,
                                     load, "温度26.5摄氏度"));
    CHECK(!sequence.environment_start && strstr(played, "10点整"));
    epaper_audio_sequence_free(&sequence);
    played[0] = 0;
    CHECK(!epaper_audio_sequence_build(&sequence, 10, false, &time_only, true, 26.5f, 62,
                                      load, "10点整"));
    CHECK(!sequence.pcm && !sequence.samples && !sequence.capacity);
    played[0] = 0;
    CHECK(!epaper_audio_sequence_build(&sequence, 10, false, &c, false, NAN, NAN,
                                      load, "温湿度暂不可用"));
    CHECK(!sequence.pcm && !sequence.samples && !sequence.capacity);
    printf("announcement tests: %u checks passed\n", checks);
    return 0;
}

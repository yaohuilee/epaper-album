#include "epaper_audio_sequence.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RATE ANNOUNCEMENT_RATE
#define MAX_SAMPLES (RATE * 40u)
static bool reserve(epaper_audio_sequence_t *s, size_t count)
{
    if (count > MAX_SAMPLES - s->samples)
        return false;
    size_t needed = s->samples + count;
    if (needed <= s->capacity)
        return true;
    size_t capacity = (needed + RATE - 1) / RATE * RATE;
    int16_t *p = realloc(s->pcm, capacity * sizeof(*p));
    if (!p)
        return false;
    s->pcm = p;
    s->capacity = capacity;
    return true;
}
static bool silence(epaper_audio_sequence_t *s, unsigned milliseconds)
{
    size_t n = (size_t)milliseconds * RATE / 1000;
    if (!reserve(s, n))
        return false;
    memset(s->pcm + s->samples, 0, n * 2);
    s->samples += n;
    return true;
}
static bool clip(epaper_audio_sequence_t *s, const char *name, epaper_speech_synthesizer_t load,
                 void *context)
{
    int16_t *pcm = NULL;
    size_t n = 0;
    if (!load(name, &pcm, &n, context) || !pcm || !n || !reserve(s, n))
    {
        free(pcm);
        return false;
    }
    memcpy(s->pcm + s->samples, pcm, n * 2);
    s->samples += n;
    free(pcm);
    return true;
}
static bool beep(epaper_audio_sequence_t *s, unsigned hz, unsigned milliseconds)
{
    size_t n = (size_t)milliseconds * RATE / 1000;
    if (!reserve(s, n))
        return false;
    for (size_t i = 0; i < n; i++)
    {
        double fade = fmin(1.0, fmin((double)i / 120, (double)(n - 1 - i) / 120));
        s->pcm[s->samples + i] =
            (int16_t)(8192.0 * fade * sin(6.283185307179586 * (double)hz * (double)i / RATE));
    }
    s->samples += n;
    return true;
}
void epaper_audio_sequence_free(epaper_audio_sequence_t *s)
{
    if (s)
    {
        free(s->pcm);
        memset(s, 0, sizeof(*s));
    }
}
bool epaper_audio_sequence_build(epaper_audio_sequence_t *s, unsigned hour, bool half,
                                 const epaper_announcement_config_t *c, bool valid,
                                 float temperature, float humidity, epaper_speech_synthesizer_t load,
                                 void *context)
{
    if (!s || !c || !load || hour > 23)
        return false;
    memset(s, 0, sizeof(*s));
    s->beep_count = half ? 3 : 5;
    for (unsigned i = 1; i < s->beep_count; i++)
        if (!beep(s, 1000, 100) || !silence(s, 900))
            goto failed;
    s->last_beep_start = s->samples;
    if (!beep(s, 1500, 250))
        goto failed;
    s->last_beep_end = s->samples;
    if (!silence(s, 1000))
        goto failed;
    s->prefix_start = s->samples;
    if (!clip(s, "现在时刻", load, context))
        goto failed;
    s->prefix_end = s->samples;
    if (!silence(s, 1000))
        goto failed;
    s->time_start = s->samples;
    char name[32];
    snprintf(name, sizeof(name), "%u点%s", hour, half ? "三十分" : "整");
    if (!clip(s, name, load, context))
        goto failed;
    s->time_end = s->samples;
    if (c->temperature || c->humidity)
    {
        if (!silence(s, 1000))
            goto failed;
        s->environment_start = s->samples;
        bool usable = valid && isfinite(temperature) && isfinite(humidity) && temperature >= -40 &&
                      temperature <= 125 && humidity >= 0 && humidity <= 100;
        if (!usable)
        {
            if (!clip(s, "温湿度暂不可用", load, context))
                goto failed;
        }
        else
        {
            int t = (int)lroundf(temperature * (c->temp_decimals ? 10 : 1));
            unsigned absolute = (unsigned)(t < 0 ? -t : t);
            char text[96], fraction[16] = "";
            if (c->temp_decimals && absolute % 10)
                snprintf(fraction, sizeof(fraction), ".%u", absolute % 10);
            if (c->temperature) {
                snprintf(text, sizeof(text), "温度%s%u%s摄氏度", t < 0 ? "零下" : "",
                         absolute / (c->temp_decimals ? 10 : 1), fraction);
                if (!clip(s, text, load, context)) goto failed;
            }
            if (c->humidity) {
                if (c->temperature && !silence(s, 300)) goto failed;
                snprintf(text, sizeof(text), "湿度百分之%u", (unsigned)lroundf(humidity));
                if (!clip(s, text, load, context)) goto failed;
            }
        }
    }
    if (!silence(s, 100))
        goto failed;
    return true;
failed:
    epaper_audio_sequence_free(s);
    return false;
}

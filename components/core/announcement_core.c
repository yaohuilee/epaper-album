#include "cJSON.h"
#include "epaper_announcement_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool fail(char *error, size_t size, const char *text)
{
    if (error && size)
        snprintf(error, size, "%s", text);
    return false;
}
bool epaper_announcement_countdown_window(int64_t lead_us)
{
    return lead_us >= 100000 && lead_us <= (ANNOUNCEMENT_PREPARE_SECONDS + 5LL) * 1000000;
}
uint64_t epaper_announcement_wake_delay(int64_t target_epoch, int64_t now_us)
{
    if (!target_epoch)
        return 0;
    int64_t remaining = (target_epoch - 30) * 1000000 - now_us;
    if (remaining <= 0)
        return 1000000;
    if (remaining <= 600000000)
        return (uint64_t)(remaining < 60000000 ? remaining : 60000000);
    remaining -= 600000000;
    return (uint64_t)(remaining < 900000000 ? remaining : 900000000);
}
static const cJSON *field(const cJSON *v, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(v, key);
}
static bool unique(const cJSON *v)
{
    for (const cJSON *a = v ? v->child : NULL; a; a = a->next)
    {
        for (const cJSON *b = a->next; cJSON_IsObject(v) && b; b = b->next)
            if (a->string && b->string && !strcmp(a->string, b->string))
                return false;
        if (!unique(a))
            return false;
    }
    return true;
}
cJSON *epaper_announcement_json_parse(const char *s, size_t limit)
{
    if (!s || strlen(s) > limit)
        return NULL;
    unsigned depth = 0;
    bool quote = false, escape = false;
    for (const char *p = s; *p; p++)
    {
        if (quote)
        {
            if (escape)
                escape = false;
            else if (*p == '\\')
            {
                if (!strncmp(p, "\\u0000", 6))
                    return NULL;
                escape = true;
            }
            else if (*p == '"')
                quote = false;
        }
        else if (*p == '"')
            quote = true;
        else if (*p == '{' || *p == '[')
        {
            if (++depth > 24)
                return NULL;
        }
        else if (*p == '}' || *p == ']')
        {
            if (!depth)
                return NULL;
            depth--;
        }
    }
    if (depth || quote)
        return NULL;
    cJSON *v = cJSON_ParseWithOpts(s, NULL, true);
    if (!cJSON_IsObject(v) || !unique(v))
    {
        cJSON_Delete(v);
        return NULL;
    }
    return v;
}
static cJSON *parse(const char *s) { return epaper_announcement_json_parse(s, 16384); }
static bool known(const cJSON *v, const char *allowed)
{
    for (const cJSON *p = v ? v->child : NULL; p; p = p->next)
    {
        char key[96];
        if (!p->string || snprintf(key, sizeof(key), "|%s|", p->string) >= (int)sizeof(key) ||
            !strstr(allowed, key))
            return false;
    }
    return true;
}
static bool boolean(const cJSON *v, const char *key, bool fallback, bool *out)
{
    const cJSON *x = field(v, key);
    if (!x)
    {
        *out = fallback;
        return true;
    }
    if (!cJSON_IsBool(x))
        return false;
    *out = cJSON_IsTrue(x);
    return true;
}
static bool integer(const cJSON *v, const char *key, unsigned fallback, unsigned max, unsigned *out)
{
    const cJSON *x = field(v, key);
    if (!x)
    {
        *out = fallback;
        return true;
    }
    if (!cJSON_IsNumber(x) || !isfinite(x->valuedouble) || x->valuedouble < 0 ||
        x->valuedouble > max || floor(x->valuedouble) != x->valuedouble)
        return false;
    *out = (unsigned)x->valuedouble;
    return true;
}
static bool copy(const cJSON *v, const char *key, const char *fallback, char *out, size_t size)
{
    const cJSON *x = field(v, key);
    const char *s = x && cJSON_IsString(x) ? x->valuestring : !x ? fallback : NULL;
    if (!s || !*s || strlen(s) >= size)
        return false;
    strcpy(out, s);
    return true;
}
static bool time_value(const cJSON *v, const char *key, unsigned fallback, unsigned *out)
{
    const cJSON *x = field(v, key);
    if (!x)
    {
        *out = fallback;
        return true;
    }
    if (!cJSON_IsString(x) || strlen(x->valuestring) != 5)
        return false;
    const char *s = x->valuestring;
    if (s[2] != ':' || s[0] < '0' || s[0] > '2' || s[1] < '0' || s[1] > '9' || s[3] < '0' ||
        s[3] > '5' || s[4] < '0' || s[4] > '9')
        return false;
    unsigned h = (s[0] - '0') * 10 + s[1] - '0';
    if (h > 23)
        return false;
    *out = h * 60 + (s[3] - '0') * 10 + s[4] - '0';
    return true;
}
bool epaper_audio_pack_name(const char *s)
{
    if (!s || !*s || strlen(s) > 48)
        return false;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
              *s == '-' || *s == '_'))
            return false;
    return true;
}
bool epaper_announcement_parse(const char *text, epaper_announcement_config_t *out, char *error,
                               size_t size)
{
    if (!out)
        return fail(error, size, "Missing output");
    cJSON *root = parse(text);
    epaper_announcement_config_t c = {0};
    bool ok = false;
    const char *reason = "Invalid announcement JSON";
    if (!root)
        goto end;
    const cJSON *schema = field(root, "schema_version");
    if (!cJSON_IsNumber(schema) || schema->valuedouble != 1)
        goto end;
    const cJSON *a = field(root, "announcement");
    if (a && !cJSON_IsObject(a))
        goto end;
    if (!boolean(a, "enabled", false, &c.enabled) ||
        !integer(a, "volume_percent", 40, 100, &c.volume) ||
        !boolean(a, "include_temperature", true, &c.temperature) ||
        !boolean(a, "include_humidity", true, &c.humidity) ||
        !integer(a, "temperature_decimals", 1, 1, &c.temp_decimals))
        goto end;
    const cJSON *minutes = field(a, "minutes");
    if (!minutes)
        c.hourly = c.half_hourly = true;
    else
    {
        if (!cJSON_IsArray(minutes) || cJSON_GetArraySize(minutes) < 1 ||
            cJSON_GetArraySize(minutes) > 2)
            goto end;
        for (const cJSON *m = minutes->child; m; m = m->next)
        {
            if (!cJSON_IsNumber(m))
                goto end;
            if (m->valuedouble == 0 && !c.hourly)
                c.hourly = true;
            else if (m->valuedouble == 30 && !c.half_hourly)
                c.half_hourly = true;
            else
                goto end;
        }
    }
    const cJSON *periods = field(a, "pause_periods"), *legacy = field(a, "quiet_hours");
    reason = "Invalid pause periods; use up to 8 distinct IDs and HH:mm ranges";
    if (periods && legacy)
        goto end;
    if (periods)
    {
        if (!cJSON_IsArray(periods) || cJSON_GetArraySize(periods) > ANNOUNCEMENT_MAX_PERIODS)
            goto end;
        for (const cJSON *p = periods->child; p; p = p->next)
        {
            epaper_pause_period_t *v = &c.periods[c.period_count];
            if (!cJSON_IsObject(p) || !known(p, "|id|name|enabled|start|end|") ||
                !copy(p, "id", NULL, v->id, sizeof(v->id)) || !epaper_audio_pack_name(v->id) ||
                !copy(p, "name", v->id, v->name, sizeof(v->name)) ||
                !boolean(p, "enabled", true, &v->enabled) || !field(p, "start") ||
                !field(p, "end") || !time_value(p, "start", 1320, &v->start) ||
                !time_value(p, "end", 360, &v->end) || v->start == v->end)
                goto end;
            for (unsigned i = 0; i < c.period_count; i++)
                if (!strcmp(v->id, c.periods[i].id))
                    goto end;
            c.period_count++;
        }
    }
    else
    {
        epaper_pause_period_t *v = &c.periods[0];
        strcpy(v->id, "night");
        strcpy(v->name, "夜间");
        if ((legacy && !cJSON_IsObject(legacy)) || !boolean(legacy, "enabled", true, &v->enabled) ||
            !time_value(legacy, "start", 1320, &v->start) ||
            !time_value(legacy, "end", 360, &v->end) || v->start == v->end)
            goto end;
        c.period_count = 1;
    }
    *out = c;
    ok = true;
    if (error && size)
        error[0] = 0;
end:
    cJSON_Delete(root);
    return ok || fail(error, size, reason);
}
bool epaper_announcement_paused(const epaper_announcement_config_t *c, int64_t epoch, int *period)
{
    if (period)
        *period = -1;
    if (!c)
        return false;
    unsigned minute = (unsigned)(((epoch + 28800) % 86400 + 86400) % 86400) / 60;
    for (unsigned i = 0; i < c->period_count; i++)
    {
        const epaper_pause_period_t *p = &c->periods[i];
        if (p->enabled && (p->start < p->end ? (minute >= p->start && minute < p->end)
                                             : (minute >= p->start || minute < p->end)))
        {
            if (period)
                *period = (int)i;
            return true;
        }
    }
    return false;
}
int64_t epaper_announcement_next(const epaper_announcement_config_t *c, int64_t minimum,
                                 int64_t processed)
{
    if (!c || !c->enabled || !c->volume || minimum < 1577836800)
        return 0;
    if (minimum <= processed)
        minimum = processed + 1;
    int64_t slot = (minimum + 1799) / 1800 * 1800;
    for (unsigned i = 0; i < 48; i++, slot += 1800)
    {
        unsigned minute = (unsigned)(slot / 60 % 60);
        if ((minute == 0 ? c->hourly : c->half_hourly) &&
            !epaper_announcement_paused(c, slot, NULL))
            return slot;
    }
    return 0;
}
static cJSON *encode(const epaper_announcement_config_t *c)
{
    cJSON *v = cJSON_CreateObject();
    if (!v)
        return NULL;
    cJSON *m = cJSON_AddArrayToObject(v, "minutes"),
          *p = cJSON_AddArrayToObject(v, "pause_periods");
    if (!m || !p || !cJSON_AddBoolToObject(v, "enabled", c->enabled) ||
        !cJSON_AddNumberToObject(v, "volume_percent", c->volume) ||
        !cJSON_AddStringToObject(v, "engine", "esp-tts") ||
        !cJSON_AddStringToObject(v, "voice", "xiaole") ||
        !cJSON_AddNumberToObject(v, "speech_speed", 2) ||
        !cJSON_AddBoolToObject(v, "include_temperature", c->temperature) ||
        !cJSON_AddBoolToObject(v, "include_humidity", c->humidity) ||
        !cJSON_AddNumberToObject(v, "temperature_decimals", c->temp_decimals) ||
        !cJSON_AddNumberToObject(v, "humidity_decimals", 0) ||
        !cJSON_AddNumberToObject(v, "max_lateness_seconds", 0) ||
        !cJSON_AddBoolToObject(v, "catch_up_missed", false))
        goto fail;
    for (unsigned minute = 0; minute <= 30; minute += 30)
    {
        if (minute == 0 ? !c->hourly : !c->half_hourly)
            continue;
        cJSON *x = cJSON_CreateNumber(minute);
        if (!x)
            goto fail;
        if (!cJSON_AddItemToArray(m, x))
        {
            cJSON_Delete(x);
            goto fail;
        }
    }
    for (unsigned i = 0; i < c->period_count; i++)
    {
        const epaper_pause_period_t *a = &c->periods[i];
        char start[6], end[6];
        snprintf(start, sizeof(start), "%02u:%02u", a->start / 60 % 24, a->start % 60);
        snprintf(end, sizeof(end), "%02u:%02u", a->end / 60 % 24, a->end % 60);
        cJSON *x = cJSON_CreateObject();
        if (!x)
            goto fail;
        if (!cJSON_AddStringToObject(x, "id", a->id) ||
            !cJSON_AddStringToObject(x, "name", a->name) ||
            !cJSON_AddBoolToObject(x, "enabled", a->enabled) ||
            !cJSON_AddStringToObject(x, "start", start) || !cJSON_AddStringToObject(x, "end", end))
        {
            cJSON_Delete(x);
            goto fail;
        }
        if (!cJSON_AddItemToArray(p, x))
        {
            cJSON_Delete(x);
            goto fail;
        }
    }
    return v;
fail:
    cJSON_Delete(v);
    return NULL;
}
bool epaper_announcement_edit(const char *global, const char *request, bool periods, char **output,
                              epaper_announcement_config_t *out, char *error, size_t size)
{
    *output = NULL;
    epaper_announcement_config_t c;
    if (!epaper_announcement_parse(global, &c, error, size))
        return false;
    cJSON *root = parse(global), *edit = parse(request);
    bool ok = false;
    if (!root || !edit || !cJSON_IsNumber(field(edit, "schema_version")) ||
        field(edit, "schema_version")->valuedouble != 1)
        goto end;
    if (!known(edit, periods ? "|schema_version|pause_periods|"
                             : "|schema_version|enabled|minutes|volume_percent|"))
        goto end;
    if (periods ? !cJSON_IsArray(field(edit, "pause_periods"))
                : (!field(edit, "enabled") || !field(edit, "minutes") ||
                   !field(edit, "volume_percent")))
        goto end;
    cJSON *a = encode(&c);
    if (!a)
        goto end;
    for (const cJSON *f = edit->child; f; f = f->next)
        if (strcmp(f->string, "schema_version"))
        {
            cJSON *copy = cJSON_Duplicate(f, true);
            if (!copy)
            {
                cJSON_Delete(a);
                goto end;
            }
            cJSON_DeleteItemFromObjectCaseSensitive(a, f->string);
            if (!cJSON_AddItemToObject(a, f->string, copy))
            {
                cJSON_Delete(copy);
                cJSON_Delete(a);
                goto end;
            }
        }
    cJSON_DeleteItemFromObjectCaseSensitive(root, "announcement");
    if (!cJSON_AddItemToObject(root, "announcement", a))
    {
        cJSON_Delete(a);
        goto end;
    }
    *output = cJSON_Print(root);
    if (!*output || strlen(*output) > 16384)
        goto end;
    ok = epaper_announcement_parse(*output, out, error, size);
end:
    cJSON_Delete(root);
    cJSON_Delete(edit);
    if (!ok)
    {
        free(*output);
        *output = NULL;
        return fail(
            error, size,
            "Invalid announcement edit; use valid switches, minutes, volume and pause ranges");
    }
    return true;
}

bool epaper_announcement_migrate(const char *global, char **output, char *error, size_t size)
{
    *output = NULL;
    epaper_announcement_config_t c;
    if (!epaper_announcement_parse(global, &c, error, size)) return false;
    cJSON *root = parse(global);
    const cJSON *old = field(root, "announcement");
    const cJSON *engine = field(old, "engine"), *voice = field(old, "voice"), *speed = field(old, "speech_speed");
    bool current = cJSON_IsString(engine) && !strcmp(engine->valuestring, "esp-tts") &&
                   cJSON_IsString(voice) && !strcmp(voice->valuestring, "xiaole") &&
                   cJSON_IsNumber(speed) && speed->valuedouble == 2 &&
                   !field(old, "voice_pack") && !field(old, "audio_root");
    if (current) { cJSON_Delete(root); return true; }
    cJSON *a = encode(&c);
    if (!a) { cJSON_Delete(root); return fail(error, size, "Cannot allocate migrated announcement"); }
    cJSON_DeleteItemFromObjectCaseSensitive(root, "announcement");
    if (!cJSON_AddItemToObject(root, "announcement", a)) {
        cJSON_Delete(a); cJSON_Delete(root); return fail(error, size, "Cannot migrate announcement");
    }
    *output = cJSON_Print(root); cJSON_Delete(root);
    if (!*output || strlen(*output) > 16384) {
        free(*output); *output = NULL; return fail(error, size, "Cannot serialize migrated announcement");
    }
    return true;
}

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int year, month, day, weekday, hour, minute, second;
    bool oscillator_stopped;
} epaper_calendar_t;

/* Decode civil calendar only; no implicit UTC/local-time assumption. */
bool epaper_calendar_decode(const uint8_t raw[7], epaper_calendar_t *out);
bool epaper_calendar_encode(const epaper_calendar_t *calendar,uint8_t raw[7]);
uint8_t epaper_crc8(const uint8_t *data, size_t length);
typedef struct {
    bool vbus_good,battery_present;
    uint8_t current_direction,charge_phase,status1,status2;
} epaper_power_t;
bool epaper_power_decode(uint8_t status1,uint8_t status2,epaper_power_t *out);
typedef struct {bool seen;int64_t last_host_us;} epaper_usb_policy_t;
/* Brief SOF gaps must not make the device sleep during a debug session. */
bool epaper_usb_keep_awake(epaper_usb_policy_t *state,bool host,int64_t now_us);

/* Unknown power status keeps the device awake until a successful read. */
bool epaper_power_keep_awake(bool usb_host,bool power_valid,bool vbus_good);

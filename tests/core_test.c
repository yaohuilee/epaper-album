#include <assert.h>
#include <stdio.h>
#include "epaper_core.h"

int main(void)
{
    epaper_calendar_t c;
    uint8_t raw[] = {0x32,0x03,0x21,0x09,0x05,0x10,0x26};
    assert(epaper_calendar_decode(raw, &c));
    assert(c.year == 2026 && c.month == 10 && c.day == 9 && c.hour == 21);
    assert(c.minute == 3 && c.second == 32 && !c.oscillator_stopped);
    raw[0] |= 0x80;
    assert(epaper_calendar_decode(raw, &c) && c.oscillator_stopped);
    raw[0] = 0x6a;
    assert(!epaper_calendar_decode(raw, &c));
    raw[0] = 0x00; raw[3] = 0x29; raw[5] = 0x02; raw[6] = 0x24;
    assert(epaper_calendar_decode(raw, &c));
    raw[6] = 0x25;
    assert(!epaper_calendar_decode(raw, &c));
    raw[3] = 0x28; raw[5] = 0;
    assert(!epaper_calendar_decode(raw, &c));
    raw[5] = 0x02; raw[4] = 7;
    assert(!epaper_calendar_decode(raw, &c));
    assert(!epaper_calendar_decode(NULL, &c));
    assert(!epaper_calendar_decode(raw, NULL));
    c=(epaper_calendar_t){.year=2028,.month=2,.day=29,.weekday=2,.hour=23,.minute=59,.second=58};
    assert(epaper_calendar_encode(&c,raw));
    assert(raw[0]==0x58 && raw[3]==0x29 && raw[6]==0x28);
    epaper_calendar_t decoded;
    assert(epaper_calendar_decode(raw,&decoded));
    assert(decoded.year==c.year && decoded.day==c.day && decoded.hour==c.hour);
    c.year=2027; assert(!epaper_calendar_encode(&c,raw));
    c.year=2100; assert(!epaper_calendar_encode(&c,raw));
    c.year=2028;c.second=60;assert(!epaper_calendar_encode(&c,raw));
    assert(!epaper_calendar_encode(NULL,raw));
    assert(!epaper_calendar_encode(&c,NULL));
    const uint8_t crc_vector[] = {0xbe,0xef};
    assert(epaper_crc8(crc_vector, 2) == 0x92);
    assert(epaper_crc8(NULL, 0) == 0xff);
    puts("Calendar validity and sensor CRC tests passed");
    epaper_power_t power;
    assert(epaper_power_decode(0x28,0x32,&power));
    assert(power.vbus_good && power.battery_present && power.current_direction==1 && power.charge_phase==2);
    assert(epaper_power_decode(0x08,0x55,&power));
    assert(!power.vbus_good && power.battery_present && power.current_direction==2 && power.charge_phase==5);
    assert(epaper_power_decode(0x20,0x14,&power));
    assert(power.vbus_good && !power.battery_present && power.current_direction==0 && power.charge_phase==4);
    assert(!epaper_power_decode(0,0,NULL));
    epaper_usb_policy_t usb={0};
    assert(!epaper_usb_keep_awake(&usb,false,10000000)); /* No active USB data connection. */
    assert(epaper_usb_keep_awake(&usb,true,11000000));
    assert(epaper_usb_keep_awake(&usb,false,13999999)); /* SOF gap tolerance. */
    assert(!epaper_usb_keep_awake(&usb,false,14000000));
    assert(epaper_usb_keep_awake(&usb,true,15000000)); /* Reconnection. */
    assert(!epaper_usb_keep_awake(&usb,false,18000000));
    assert(epaper_power_keep_awake(false,true,true)); /* Charger, no USB host. */
    assert(epaper_power_keep_awake(true,true,true)); /* Computer. */
    assert(epaper_power_keep_awake(true,true,false)); /* USB disconnect grace. */
    assert(!epaper_power_keep_awake(false,true,false)); /* Battery. */
    assert(epaper_power_keep_awake(false,false,false)); /* PMIC read failure. */
    assert(epaper_power_keep_awake(false,false,true));
    puts("Power decoding, USB grace and external-power sleep policy tests passed");
    return 0;
}

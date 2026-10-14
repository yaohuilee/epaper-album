#include "epaper_core.h"

bool epaper_power_decode(uint8_t status1,uint8_t status2,epaper_power_t *out)
{
    if(!out) return false;
    *out=(epaper_power_t){.vbus_good=(status1&0x20)!=0,.battery_present=(status1&0x08)!=0,
        .current_direction=(status2>>5)&3,.charge_phase=status2&7,.status1=status1,.status2=status2};
    return true;
}
bool epaper_usb_keep_awake(epaper_usb_policy_t *state,bool host,int64_t now_us)
{
    if(!state) return host;
    if(host) {state->seen=true;state->last_host_us=now_us;return true;}
    return state->seen && now_us-state->last_host_us<3000000;
}

bool epaper_power_keep_awake(bool usb_host,bool power_valid,bool vbus_good)
{return usb_host || !power_valid || vbus_good;}

static bool bcd(uint8_t value, int *out)
{
    if ((value & 15) > 9 || (value >> 4) > 9) return false;
    *out = (value >> 4) * 10 + (value & 15);
    return true;
}

bool epaper_calendar_decode(const uint8_t raw[7], epaper_calendar_t *out)
{
    if (!raw || !out) return false;
    epaper_calendar_t c = { .oscillator_stopped = (raw[0] & 0x80) != 0 };
    if (!bcd(raw[0] & 0x7f, &c.second) || !bcd(raw[1] & 0x7f, &c.minute) ||
        !bcd(raw[2] & 0x3f, &c.hour) || !bcd(raw[3] & 0x3f, &c.day) ||
        !bcd(raw[5] & 0x1f, &c.month) || !bcd(raw[6], &c.year)) return false;
    c.year += 2000;
    c.weekday = raw[4] & 7;
    if (c.second > 59 || c.minute > 59 || c.hour > 23 || c.weekday > 6 ||
        c.month < 1 || c.month > 12 || c.day < 1) return false;
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int limit = days[c.month - 1];
    if (c.month == 2 && c.year % 4 == 0) limit++; /* PCF calendar: 2000..2099 */
    if (c.day > limit) return false;
    *out = c;
    return true;
}

uint8_t epaper_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xff;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (uint8_t)((crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1);
        }
    }
    return crc;
}
bool epaper_calendar_encode(const epaper_calendar_t *c,uint8_t raw[7])
{
    if(!c || !raw || c->year<2000 || c->year>2099 || c->oscillator_stopped ||
       c->second<0 || c->second>59 || c->minute<0 || c->minute>59 ||
       c->hour<0 || c->hour>23 || c->day<1 || c->day>31 ||
       c->weekday<0 || c->weekday>6 || c->month<1 || c->month>12) return false;
    int values[7]={c->second,c->minute,c->hour,c->day,c->weekday,c->month,c->year-2000};
    uint8_t encoded[7];
    for(unsigned i=0;i<7;i++) encoded[i]=(uint8_t)((values[i]/10)*16+values[i]%10);
    epaper_calendar_t check;
    if(!epaper_calendar_decode(encoded,&check)) return false;
    for(unsigned i=0;i<7;i++) raw[i]=encoded[i];
    return true;
}
bool epaper_calendar_epoch_utc(const epaper_calendar_t *c,int64_t *epoch)
{
    uint8_t raw[7];
    if(!epoch||!epaper_calendar_encode(c,raw))return false;
    int64_t days=10957;
    for(int year=2000;year<c->year;year++)days+=year%4==0?366:365;
    static const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    for(int month=1;month<c->month;month++)days+=lengths[month-1]+(month==2&&c->year%4==0);
    *epoch=(days+c->day-1)*86400+c->hour*3600+c->minute*60+c->second;
    return true;
}

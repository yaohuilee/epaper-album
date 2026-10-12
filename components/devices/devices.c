#include "epaper_devices.h"
#include "epaper_board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t epaper_power_read(epaper_power_t *out)
{
    if(!out) return ESP_ERR_INVALID_ARG;
    *out=(epaper_power_t){0};
    uint8_t raw[2];
    esp_err_t err=epaper_board_read_reg(BOARD_PMIC,0x00,raw,sizeof(raw));
    if(err==ESP_OK) epaper_power_decode(raw[0],raw[1],out);
    return err;
}

esp_err_t epaper_rtc_read(epaper_rtc_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = (epaper_rtc_t){0};
    uint8_t control, raw[7];
    esp_err_t err = epaper_board_read_reg(BOARD_RTC, 0x00, &control, 1);
    if (err != ESP_OK) return err;
    err = epaper_board_read_reg(BOARD_RTC, 0x04, raw, sizeof(raw));
    if (err != ESP_OK) return err;
    if (!epaper_calendar_decode(raw, &out->calendar)) return ESP_ERR_INVALID_RESPONSE;
    out->stopped = (control & 0x20) != 0;
    out->valid = !out->stopped && !out->calendar.oscillator_stopped;
    return ESP_OK;
}

static esp_err_t sht_command(uint16_t command)
{
    uint8_t tx[] = {(uint8_t)(command >> 8), (uint8_t)command};
    return epaper_board_transfer(BOARD_SHTC3, tx, sizeof(tx), NULL, 0);
}
esp_err_t epaper_rtc_write(const epaper_calendar_t *utc)
{
    uint8_t tx[8]={0x04},control;
    if(!epaper_calendar_encode(utc,tx+1)) return ESP_ERR_INVALID_ARG;
    esp_err_t err=epaper_board_read_reg(BOARD_RTC,0,&control,1);
    if(err!=ESP_OK) return err;
    uint8_t stop[2]={0,(uint8_t)(control|0x20)};
    err=epaper_board_transfer(BOARD_RTC,stop,2,NULL,0);
    if(err!=ESP_OK) return err;
    err=epaper_board_transfer(BOARD_RTC,tx,sizeof(tx),NULL,0);
    uint8_t run[2]={0,(uint8_t)(control&~0x20)};
    esp_err_t restart=epaper_board_transfer(BOARD_RTC,run,2,NULL,0);
    return err==ESP_OK?restart:err;
}

/* One application task owns the wake/measure/read/sleep sequence. */
esp_err_t epaper_environment_read(epaper_environment_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = (epaper_environment_t){0};
    esp_err_t err = sht_command(0x3517);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t raw[6];
    err = sht_command(0x7866); /* temperature first, normal mode, no clock stretching */
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
        err = epaper_board_transfer(BOARD_SHTC3, NULL, 0, raw, sizeof(raw));
    }
    esp_err_t sleep_err = sht_command(0xb098);
    if (err != ESP_OK) return err;
    if (sleep_err != ESP_OK) return sleep_err;
    if (epaper_crc8(raw, 2) != raw[2] || epaper_crc8(raw + 3, 2) != raw[5])
        return ESP_ERR_INVALID_CRC;
    unsigned temperature = ((unsigned)raw[0] << 8) | raw[1];
    unsigned humidity = ((unsigned)raw[3] << 8) | raw[4];
    out->temperature_c = -45.0f + 175.0f * temperature / 65536.0f;
    out->humidity_percent = 100.0f * humidity / 65536.0f;
    return ESP_OK;
}

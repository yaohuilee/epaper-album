#include "epaper_board.h"
#include "epaper_bmp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static esp_err_t transfer(const uint8_t *data,size_t length)
{
    spi_device_handle_t spi=epaper_board_display_spi();
    if(!spi) return ESP_ERR_INVALID_STATE;
    uint8_t scratch[4096]; /* DMA-safe internal task stack, not the PSRAM framebuffer */
    while(length) {
        size_t size=length>sizeof(scratch)?sizeof(scratch):length;
        memcpy(scratch,data,size);
        spi_transaction_t transaction={.length=size*8,.tx_buffer=scratch};
        esp_err_t err=spi_device_polling_transmit(spi,&transaction);
        if(err!=ESP_OK) return err;
        data+=size;length-=size;
    }
    return ESP_OK;
}
static esp_err_t command(uint8_t cmd,const uint8_t *data,size_t length)
{
    gpio_set_level(BOARD_EPD_CS,0);gpio_set_level(BOARD_EPD_DC,0);
    esp_err_t err=transfer(&cmd,1);
    gpio_set_level(BOARD_EPD_DC,1);
    if(err==ESP_OK && length) err=transfer(data,length);
    gpio_set_level(BOARD_EPD_CS,1);
    return err;
}
static esp_err_t ready(void)
{
    int64_t end=esp_timer_get_time()+45000000;
    while(!gpio_get_level(BOARD_EPD_BUSY)) {
        if(esp_timer_get_time()>end) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(150));return ESP_OK;
}
esp_err_t epaper_display_frame(const uint8_t *frame)
{
    if(!frame) return ESP_ERR_INVALID_ARG;
    gpio_set_level(BOARD_EPD_RST,1);vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(BOARD_EPD_RST,0);vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(BOARD_EPD_RST,1);vTaskDelay(pdMS_TO_TICKS(50));
    esp_err_t err=ready();if(err!=ESP_OK) return err;
    /* Register values from the board's independently tested panel sequence. */
    static const struct {uint8_t cmd,length,data[6];} sequence[]={
        {0xaa,6,{0x49,0x55,0x20,0x08,0x09,0x18}}, {0x01,1,{0x3f}},
        {0x00,2,{0x5f,0x69}}, {0x03,4,{0x00,0x54,0x00,0x44}},
        {0x05,4,{0x40,0x1f,0x1f,0x2c}}, {0x06,4,{0x6f,0x1f,0x17,0x49}},
        {0x08,4,{0x6f,0x1f,0x1f,0x22}}, {0x30,1,{0x03}}, {0x50,1,{0x3f}},
        {0x60,2,{0x02,0x00}}, {0x61,4,{0x03,0x20,0x01,0xe0}},
        {0x84,1,{0x01}}, {0xe3,1,{0x2f}}, {0x04,0,{0}},
    };
    for(size_t i=0;i<sizeof(sequence)/sizeof(sequence[0]);i++) {
        err=command(sequence[i].cmd,sequence[i].data,sequence[i].length);
        if(err!=ESP_OK) goto cleanup;
    }
    err=ready();if(err!=ESP_OK) goto cleanup;
    err=command(0x10,frame,EPAPER_FRAME_BYTES);if(err!=ESP_OK) goto cleanup;
    err=command(0x04,NULL,0);if(err!=ESP_OK) goto cleanup;
    err=ready();if(err!=ESP_OK) goto cleanup;
    const uint8_t boost[]={0x6f,0x1f,0x17,0x49},zero=0;
    err=command(0x06,boost,sizeof(boost));if(err!=ESP_OK) goto cleanup;
    int64_t start=esp_timer_get_time();
    err=command(0x12,&zero,1);if(err!=ESP_OK) goto cleanup;
    err=ready();
    /* BUSY must really represent a refresh, not an unpowered panel. */
    if(err==ESP_OK && esp_timer_get_time()-start<1000000) err=ESP_ERR_INVALID_RESPONSE;
    if(err==ESP_OK) ESP_LOGI("display","Refresh completed in %.2f seconds",(esp_timer_get_time()-start)/1000000.0);
cleanup:
    {
        const uint8_t off=0;
        esp_err_t off_err=command(0x02,&off,1);
        if(off_err==ESP_OK) off_err=ready();
        return err==ESP_OK?off_err:err;
    }
}

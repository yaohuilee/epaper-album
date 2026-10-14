/* SPDX-License-Identifier: Apache-2.0
 * ES8311 initialization parameters adapted from Espressif esp-bsp/es8311.
 * Copyright 2015-2026 Espressif Systems (Shanghai) CO LTD.
 * PhotoPainter transport and playback adaptation Copyright 2026 yaohuilee.
 * See THIRD_PARTY_NOTICES.md and third_party/licenses/ESP-IDF-Apache-2.0.txt.
 */
#include "driver/i2s_std.h"
#include "epaper_audio_output.h"
#include "epaper_board.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
static i2s_chan_handle_t tx;
static esp_err_t reg(uint8_t address, uint8_t value)
{
    uint8_t data[] = {address, value};
    return epaper_board_transfer(BOARD_DAC, data, 2, NULL, 0);
}
void epaper_audio_output_close(void)
{
    gpio_set_level(BOARD_AUDIO_ENABLE, 0);
    if (tx)
    {
        i2s_channel_disable(tx);
        i2s_del_channel(tx);
        tx = NULL;
    }
    reg(0x31, 0x60);
    reg(0x00, 0x1f);
}
esp_err_t epaper_audio_output_prepare(void)
{
    if (tx)
        return ESP_ERR_INVALID_STATE;
    gpio_set_level(BOARD_AUDIO_ENABLE, 0);
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.dma_desc_num = 4;
    channel.dma_frame_num = 256;
    channel.auto_clear_after_cb = true;
    esp_err_t err = i2s_new_channel(&channel, &tx, NULL);
    if (err != ESP_OK)
        return err;
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000),
        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = BOARD_I2S_MCLK,
                     .bclk = BOARD_I2S_BCLK,
                     .ws = BOARD_I2S_WS,
                     .dout = BOARD_I2S_TX,
                     .din = I2S_GPIO_UNUSED},
    };
    cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    err = i2s_channel_init_std_mode(tx, &cfg);
    if (err != ESP_OK)
        goto fail;
    err = i2s_channel_enable(tx);
    if (err != ESP_OK)
        goto fail;
    static const uint8_t zeros[2048] = {0};
    size_t primed = 0;
    err = i2s_channel_write(tx, zeros, sizeof(zeros), &primed, 1000);
    if (err != ESP_OK || primed != sizeof(zeros))
        goto fail;
    if ((err = reg(0, 0x1f)) != ESP_OK)
        goto fail;
    vTaskDelay(pdMS_TO_TICKS(20));
    /* 6.144 MHz / 24 kHz, 32-bit Philips stereo slots; ADC input is unused. */
    static const uint8_t sequence[][2] = {
        {0x00, 0x00}, {0x00, 0x80}, {0x01, 0x3f}, {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10},
        {0x05, 0x00}, {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xff}, {0x09, 0x10}, {0x0a, 0x10},
        {0x0d, 0x01}, {0x0e, 0x02}, {0x12, 0x00}, {0x13, 0x10}, {0x14, 0x1a}, {0x1c, 0x6a},
        {0x37, 0x08}, {0x32, 0xb3}, {0x31, 0x00},
    };
    for (size_t i = 0; i < sizeof(sequence) / sizeof(sequence[0]); i++)
        if ((err = reg(sequence[i][0], sequence[i][1])) != ESP_OK)
            goto fail;
    vTaskDelay(pdMS_TO_TICKS(30));
    return ESP_OK;
fail:
    epaper_audio_output_close();
    return err;
}
static void convert(int32_t *out, const int16_t *pcm, size_t count, unsigned volume)
{
    for (size_t i = 0; i < count; i++)
    {
        int32_t sample = (int32_t)pcm[i] * 65536;
        sample = (int32_t)((int64_t)sample * volume / 100);
        out[2 * i] = out[2 * i + 1] = sample;
    }
}
esp_err_t epaper_audio_output_play(const int16_t *pcm, size_t samples, unsigned volume,
                                   int64_t start_us)
{
    if (!tx || !pcm || !samples || volume > 100)
        return ESP_ERR_INVALID_ARG;
    int32_t *buffer = heap_caps_malloc(512 * 8, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buffer)
        return ESP_ERR_NO_MEM;
    size_t first = samples < 512 ? samples : 512, loaded = 0;
    convert(buffer, pcm, first, volume);
    gpio_set_level(BOARD_AUDIO_ENABLE, volume ? 1 : 0);
    /* Warm clocks/analog paths stay active on silence; preload only immediately before start. */
    while (esp_timer_get_time() < start_us - 2000)
    {
        int64_t remaining = start_us - 2000 - esp_timer_get_time();
        if (remaining > 20000)
            vTaskDelay(1); /* 1 ms rounds to zero at this project's 100 Hz tick. */
        else
            esp_rom_delay_us(100);
    }
    esp_err_t err = i2s_channel_disable(tx);
    if (err != ESP_OK)
    {
        free(buffer);
        return err;
    }
    err = i2s_channel_preload_data(tx, buffer, first * 8, &loaded);
    if (err != ESP_OK || loaded != first * 8)
    {
        free(buffer);
        return err == ESP_OK ? ESP_FAIL : err;
    }
    while (esp_timer_get_time() < start_us)
    {
        int64_t remaining = start_us - esp_timer_get_time();
        if (remaining > 2000)
            vTaskDelay(pdMS_TO_TICKS(1));
    }
    err = i2s_channel_enable(tx);
    if (err != ESP_OK)
    {
        free(buffer);
        return err;
    }
    for (size_t pos = first; pos < samples && err == ESP_OK;)
    {
        size_t count = samples - pos;
        if (count > 512)
            count = 512;
        convert(buffer, pcm + pos, count, volume);
        size_t written = 0;
        err = i2s_channel_write(tx, buffer, count * 8, &written, 1000);
        if (err == ESP_OK && written != count * 8)
            err = ESP_FAIL;
        pos += count;
    }
    /* write() queues DMA; allow every descriptor to drain before power-off. */
    if (err == ESP_OK)
        vTaskDelay(pdMS_TO_TICKS(60));
    free(buffer);
    return err;
}

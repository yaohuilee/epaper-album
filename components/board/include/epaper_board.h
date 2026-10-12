#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

enum {
    BOARD_I2C_SDA = 47, BOARD_I2C_SCL = 48,
    BOARD_EPD_MOSI = 11, BOARD_EPD_CLK = 10, BOARD_EPD_DC = 8,
    BOARD_EPD_CS = 9, BOARD_EPD_RST = 12, BOARD_EPD_BUSY = 13,
    BOARD_I2S_MCLK = 14, BOARD_I2S_BCLK = 15, BOARD_I2S_WS = 16,
    BOARD_I2S_TX = 17, BOARD_I2S_RX = 18, BOARD_AUDIO_ENABLE = 7,
    BOARD_SD_CLK = 39, BOARD_SD_CMD = 41, BOARD_SD_D0 = 40,
    BOARD_SD_D1 = 1, BOARD_SD_D2 = 2, BOARD_SD_D3 = 38,
    BOARD_BOOT = 0, BOARD_KEY = 4, BOARD_PWR_EVENT = 5, BOARD_PMIC_IRQ = 21,
    BOARD_LED_1 = 42, BOARD_LED_2 = 45,
    BOARD_EPD_WIDTH = 800, BOARD_EPD_HEIGHT = 480,
};

typedef enum {
    BOARD_DAC, BOARD_PMIC, BOARD_MIC_ADC, BOARD_RTC, BOARD_SHTC3,
    BOARD_DEVICE_COUNT
} board_device_t;

/* Startup only, not a concurrent/reentrant resource manager. */
esp_err_t epaper_board_init(void);
esp_err_t epaper_board_probe(board_device_t device);
esp_err_t epaper_board_transfer(board_device_t device,
    const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length);
esp_err_t epaper_board_read_reg(board_device_t device, uint8_t reg,
    uint8_t *data, size_t length);
spi_device_handle_t epaper_board_display_spi(void);

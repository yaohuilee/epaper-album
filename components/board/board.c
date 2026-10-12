#include "epaper_board.h"
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "board";
static const uint8_t addresses[] = {0x18, 0x34, 0x40, 0x51, 0x70};
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t devices[BOARD_DEVICE_COUNT];
static spi_device_handle_t display_spi;
static SemaphoreHandle_t bus_lock;
static bool init_attempted;

esp_err_t epaper_board_transfer(board_device_t device,
    const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length)
{
    if (device < 0 || device >= BOARD_DEVICE_COUNT ||
        (tx_length && !tx) || (rx_length && !rx) || (!tx_length && !rx_length))
        return ESP_ERR_INVALID_ARG;
    if (!bus_lock || !devices[device]) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(bus_lock, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err;
    if (tx_length && rx_length) {
        err = i2c_master_transmit_receive(devices[device], tx, tx_length, rx, rx_length, 100);
    } else if (tx_length) {
        err = i2c_master_transmit(devices[device], tx, tx_length, 100);
    } else {
        err = i2c_master_receive(devices[device], rx, rx_length, 100);
    }
    xSemaphoreGive(bus_lock);
    return err;
}

esp_err_t epaper_board_read_reg(board_device_t device, uint8_t reg,
    uint8_t *data, size_t length)
{
    return epaper_board_transfer(device, &reg, 1, data, length);
}

esp_err_t epaper_board_probe(board_device_t device)
{
    if (device < 0 || device >= BOARD_DEVICE_COUNT) return ESP_ERR_INVALID_ARG;
    if (!bus || !bus_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(bus_lock, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = i2c_master_probe(bus, addresses[device], 100);
    xSemaphoreGive(bus_lock);
    return err;
}

static esp_err_t pmic_update(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t old;
    esp_err_t err = epaper_board_read_reg(BOARD_PMIC, reg, &old, 1);
    if (err != ESP_OK) return err;
    uint8_t expected = (uint8_t)((old & ~mask) | (value & mask));
    if (old != expected) {
        uint8_t tx[] = {reg, expected};
        err = epaper_board_transfer(BOARD_PMIC, tx, sizeof(tx), NULL, 0);
        if (err != ESP_OK) return err;
    }
    uint8_t actual;
    err = epaper_board_read_reg(BOARD_PMIC, reg, &actual, 1);
    if (err == ESP_OK && actual != expected) return ESP_ERR_INVALID_RESPONSE;
    return err;
}

static esp_err_t power_init(void)
{
    uint8_t id;
    esp_err_t err = epaper_board_read_reg(BOARD_PMIC, 0x03, &id, 1);
    if (err != ESP_OK) return err;
    if (id != 0x4a) return ESP_ERR_INVALID_RESPONSE;
    /* ALDO1..4: 500 mV + 28 * 100 mV = 3.3 V; preserve reserved bits.
       Do not change MCU DCDC, charging parameters, power-key policy or IRQs. */
    for (uint8_t reg = 0x92; reg <= 0x95; reg++) {
        err = pmic_update(reg, 0x1f, 28);
        if (err != ESP_OK) return err;
    }
    err = pmic_update(0x90, 0x0f, 0x0f);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "AXP2101 ALDO1..4 enabled at 3.3 V (readback checked)");
    return ESP_OK;
}

spi_device_handle_t epaper_board_display_spi(void) { return display_spi; }

esp_err_t epaper_board_init(void)
{
    if (init_attempted) return ESP_ERR_INVALID_STATE;
    init_attempted = true;
    gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << BOARD_AUDIO_ENABLE) | (1ULL << BOARD_EPD_CS) |
                       (1ULL << BOARD_EPD_DC) | (1ULL << BOARD_EPD_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&outputs);
    if (err != ESP_OK) return err;
    gpio_set_level(BOARD_AUDIO_ENABLE, 0);
    gpio_set_level(BOARD_EPD_CS, 1);
    gpio_set_level(BOARD_EPD_DC, 1);
    gpio_set_level(BOARD_EPD_RST, 1);
    gpio_config_t keys = {
        .pin_bit_mask = (1ULL << BOARD_BOOT) | (1ULL << BOARD_KEY),
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    err = gpio_config(&keys);
    if (err != ESP_OK) return err;
    gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << BOARD_PWR_EVENT) | (1ULL << BOARD_PMIC_IRQ) |
                       (1ULL << BOARD_EPD_BUSY), .mode = GPIO_MODE_INPUT,
    };
    err = gpio_config(&inputs);
    if (err != ESP_OK) return err;
    bus_lock = xSemaphoreCreateMutex();
    if (!bus_lock) return ESP_ERR_NO_MEM;
    i2c_master_bus_config_t i2c = {
        .i2c_port = I2C_NUM_0, .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL, .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    err = i2c_new_master_bus(&i2c, &bus);
    if (err != ESP_OK) return err;
    for (int i = 0; i < BOARD_DEVICE_COUNT; i++) {
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addresses[i], .scl_speed_hz = 100000,
            .scl_wait_us = 20000,
        };
        err = i2c_master_bus_add_device(bus, &cfg, &devices[i]);
        if (err != ESP_OK) return err;
    }
    err = power_init();
    if (err != ESP_OK) return err;
    spi_bus_config_t spi = {
        .mosi_io_num = BOARD_EPD_MOSI, .miso_io_num = -1,
        .sclk_io_num = BOARD_EPD_CLK, .quadwp_io_num = -1,
        .quadhd_io_num = -1, .data4_io_num = -1, .data5_io_num = -1,
        .data6_io_num = -1, .data7_io_num = -1, .max_transfer_sz = 4096,
    };
    err = spi_bus_initialize(SPI2_HOST, &spi, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;
    spi_device_interface_config_t display = {
        .clock_speed_hz = 10000000, .mode = 0,
        .spics_io_num = -1, .queue_size = 1, /* manual CS keeps complete EPD command/data phases together */
    };
    return spi_bus_add_device(SPI2_HOST, &display, &display_spi);
}

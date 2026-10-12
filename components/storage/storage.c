#include "epaper_storage.h"
#include "epaper_board.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

static sdmmc_card_t *card;
static bool mount_attempted;

esp_err_t epaper_storage_mount(void)
{
    if (mount_attempted) return ESP_ERR_INVALID_STATE;
    mount_attempted = true;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; /* conservative 20 MHz bring-up */
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BOARD_SD_CLK; slot.cmd = BOARD_SD_CMD;
    slot.d0 = BOARD_SD_D0; slot.d1 = BOARD_SD_D1;
    slot.d2 = BOARD_SD_D2; slot.d3 = BOARD_SD_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t cfg = {
        .format_if_mount_failed = false, .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    return esp_vfs_fat_sdmmc_mount(EPAPER_SD_MOUNT, &host, &slot, &cfg, &card);
}

esp_err_t epaper_storage_unmount(void)
{
    if(!card) return ESP_ERR_INVALID_STATE;
    esp_err_t err=esp_vfs_fat_sdcard_unmount(EPAPER_SD_MOUNT,card);
    if(err==ESP_OK) {card=NULL;mount_attempted=false;}
    return err;
}

esp_err_t epaper_config_read(cJSON **out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    if (!card) return ESP_ERR_INVALID_STATE;
    FILE *file = fopen(EPAPER_SD_MOUNT "/config.json", "rb");
    if (!file) return ESP_ERR_NOT_FOUND;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return ESP_FAIL; }
    long size = ftell(file);
    if (size <= 0 || size > 16 * 1024) { fclose(file); return ESP_ERR_INVALID_SIZE; }
    if (fseek(file, 0, SEEK_SET) != 0) { fclose(file); return ESP_FAIL; }
    char *data = malloc((size_t)size + 1);
    if (!data) { fclose(file); return ESP_ERR_NO_MEM; }
    size_t count = fread(data, 1, (size_t)size, file);
    bool read_error = ferror(file) != 0;
    fclose(file);
    if (read_error || count != (size_t)size) { free(data); return ESP_FAIL; }
    data[count] = 0;
    /* Embedded NUL must not hide an unparsed suffix. */
    for (size_t i = 0; i < count; i++) {
        if (data[i] == 0) { free(data); return ESP_ERR_INVALID_RESPONSE; }
    }
    cJSON *root = cJSON_ParseWithLengthOpts(data, count + 1, NULL, true);
    free(data);
    if (!root) return ESP_ERR_INVALID_RESPONSE;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (!cJSON_IsObject(root) || !cJSON_IsNumber(version) || version->valuedouble != 1) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_SUPPORTED;
    }
    *out = root;
    return ESP_OK;
}

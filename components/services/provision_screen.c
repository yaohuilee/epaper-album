#include "epaper_network.h"
#include "epaper_runtime.h"
#include "provision_render.h"
#include <stdlib.h>
esp_err_t epaper_provision_screen(void)
{
    uint8_t *frame=malloc(EPAPER_FRAME_BYTES);if(!frame) return ESP_ERR_NO_MEM;
    bool valid=epaper_provision_render(frame,epaper_network_ap_ssid());
    esp_err_t err=valid?epaper_album_mark_display_dirty():ESP_FAIL;
    if(err==ESP_OK) err=epaper_display_frame(frame);
    free(frame);return err;
}

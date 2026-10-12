#pragma once
#include "epaper_album.h"
#include "esp_err.h"
/* Application task owns network lifecycle; event handlers own connection bits. */
esp_err_t epaper_network_start(const epaper_time_config_t *settings);
void epaper_network_stop(void);
bool epaper_network_online(void);
bool epaper_network_ip(char *out,size_t size);
/* Open AP survives station connection attempts; app owns provisioning lifecycle. */
esp_err_t epaper_network_provision_start(void);
bool epaper_network_provisioning(void);
const char *epaper_network_ap_ssid(void);
void epaper_network_provision_complete(void);
uint32_t epaper_network_provision_remaining_seconds(void);
bool epaper_network_provision_finish_due(void);
esp_err_t epaper_network_provision_finish(void);
esp_err_t epaper_provision_screen(void);
esp_err_t epaper_captive_dns_start(void);
void epaper_captive_dns_stop(void);

#include "esp_wifi.h"
/* App task invokes scans while it owns the network lifecycle. */
esp_err_t epaper_network_scan(wifi_ap_record_t *records,uint16_t *count,uint16_t *total);

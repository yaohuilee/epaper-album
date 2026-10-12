#pragma once
#include "esp_http_server.h"
#include <stdbool.h>
bool epaper_http_authorized(httpd_req_t *req);
bool epaper_http_file_begin(void);
void epaper_http_file_end(bool changed);
esp_err_t epaper_http_register_files(httpd_handle_t server);

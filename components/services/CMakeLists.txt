idf_component_register(SRCS "services.c" "display.c" "runtime.c" "time_sync.c" "wifi_provision.c" "network.c" "captive_dns.c" "provision_screen.c" "provision_render.c" "qrcodegen.c" "http_status.c" "http_files.c" INCLUDE_DIRS "include"
    REQUIRES board album core devices storage nvs_flash esp_timer freertos esp_wifi esp_netif esp_event esp_http_server esp_app_format esp_psram esp_hw_support spi_flash fatfs
    EMBED_TXTFILES "status.html" "albums.html" "ui.css" "ui.js" "setup.html")
if(CONFIG_EPAPER_PROVISION_WIFI)
    if(NOT EXISTS "${PROJECT_DIR}/.local/wifi_credentials.h")
        message(FATAL_ERROR "Import WIFI.txt with tools/import_wifi.py before enabling provisioning")
    endif()
    target_include_directories(${COMPONENT_LIB} PRIVATE "${PROJECT_DIR}/.local")
endif()

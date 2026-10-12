idf_component_register(SRCS "app_main.c" INCLUDE_DIRS "."
    REQUIRES board devices storage services esp_psram esp_timer esp_driver_usb_serial_jtag)

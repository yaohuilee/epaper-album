idf_component_register(SRCS "devices.c" "audio_output.c" INCLUDE_DIRS "include"
    REQUIRES board core freertos esp_driver_i2s esp_timer)

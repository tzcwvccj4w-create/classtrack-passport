idf_component_register(
    SRCS "main.c"
         "ct_adpcm.c"
         "ct_crc32.c"
         "ct_store.c"
         "ct_store_esp.c"
         "ct_classes.c"
         "ct_protocol.c"
         "ct_ui.c"
         "ct_audio.c"
         "../assets/fonts/classtrack_font_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_driver_usb_serial_jtag esp_partition esp_timer mbedtls nvs_flash
)

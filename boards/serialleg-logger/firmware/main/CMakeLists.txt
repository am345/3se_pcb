idf_component_register(SRCS "main.c" "logger.c" "link_frame.c" "preroll.c" "sd_store.c" "ui.c" "usb_link.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_app_format esp_ringbuf esp_timer fatfs sdmmc usb)

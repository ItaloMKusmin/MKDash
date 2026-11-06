idf_component_register(SRCS "src/mk_bsp.c"
                       INCLUDE_DIRS "inc"
                       REQUIRES lvgl__lvgl
                       PRIV_REQUIRES esp_lcd esp_driver_i2c esp_driver_ledc esp_driver_gpio esp_mm
                                     espressif__esp_lcd_jd9165 espressif__esp_lcd_touch
                                     espressif__esp_lcd_touch_gt911 espressif__esp_lvgl_port)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)

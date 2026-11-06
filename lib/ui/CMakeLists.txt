idf_component_register(SRCS "src/mk_ui.c"
                            "src/mk_ui_picker.c"
                            "src/widgets/mk_widget_rpm.c"
                            "src/widgets/mk_widget_value_card.c"
                            "src/fonts/mk_font_num_150.c"
                            "src/fonts/mk_font_num_80.c"
                            "src/fonts/mk_font_num_52.c"
                            "src/assets/mk_logo_splash.c"
                            "src/assets/mk_logo_img.c"
                            "src/fonts/mk_font_text_20.c"
                            "src/fonts/mk_font_text_28.c"
                       INCLUDE_DIRS "inc"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES telemetry
                       PRIV_REQUIRES lvgl__lvgl esp_timer)
# Generated font sources are excluded from the strict flags.
set_source_files_properties(src/mk_ui.c src/mk_ui_picker.c
                            src/widgets/mk_widget_rpm.c src/widgets/mk_widget_value_card.c
                            PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wformat=2;-Wshadow")

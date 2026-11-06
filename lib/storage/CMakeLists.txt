idf_component_register(SRCS "src/mk_storage.c"
                       INCLUDE_DIRS "inc"
                       REQUIRES esp_common
                       PRIV_REQUIRES nvs_flash log)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)

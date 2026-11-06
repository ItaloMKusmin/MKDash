idf_component_register(SRCS "src/mk_derived.c"
                       INCLUDE_DIRS "inc"
                       REQUIRES gear
                       PRIV_REQUIRES telemetry freertos esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)

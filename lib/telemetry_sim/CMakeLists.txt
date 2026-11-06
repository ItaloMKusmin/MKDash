idf_component_register(SRCS "src/mk_telemetry_sim.c"
                       INCLUDE_DIRS "inc"
                       PRIV_REQUIRES telemetry freertos esp_timer log)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)

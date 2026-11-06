idf_component_register(SRCS "src/mk_telemetry.c"
                       INCLUDE_DIRS "inc"
                       PRIV_REQUIRES freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)

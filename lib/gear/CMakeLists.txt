idf_component_register(SRCS "src/mk_gear.c"
                       INCLUDE_DIRS "inc")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)

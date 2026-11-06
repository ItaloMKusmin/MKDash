set(mk_sources "app_main.c" "mk_build_info.c" "mk_bringup_screen.c")
set(mk_requires esp_app_format esp_common log heap bsp_guition storage telemetry telemetry_sim ui gear derived)
if(MK_BUILD_TESTS)
    list(APPEND mk_sources "../test/test_build_info.c")
endif()

idf_component_register(SRCS ${mk_sources}
                       INCLUDE_DIRS "../inc"
                       PRIV_REQUIRES ${mk_requires})
if(MK_BUILD_TESTS)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::unity)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MK_BUILD_TESTS=1)
endif()

set(mk_git_commit "unknown")
set(mk_git_dirty "unknown")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
                    WORKING_DIRECTORY "${PROJECT_DIR}"
                    OUTPUT_VARIABLE mk_git_head OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE mk_git_result ERROR_QUIET)
    if(mk_git_result EQUAL 0)
        set(mk_git_commit "${mk_git_head}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
                        WORKING_DIRECTORY "${PROJECT_DIR}"
                        OUTPUT_VARIABLE mk_git_status RESULT_VARIABLE mk_status_result ERROR_QUIET)
        if(mk_status_result EQUAL 0)
            if(mk_git_status STREQUAL "")
                set(mk_git_dirty "false")
            else()
                set(mk_git_dirty "true")
            endif()
        endif()
    endif()
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MK_BUILD_COMMIT="${mk_git_commit}" MK_BUILD_DIRTY="${mk_git_dirty}")
set_target_properties(${COMPONENT_LIB} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c11 -Wall -Wextra -Werror -Wformat=2 -Wshadow)

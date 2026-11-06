cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.1.0-dev")
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/lib")
set(COMPONENTS src)
option(MK_BUILD_TESTS "Run bootstrap Unity tests at boot" OFF)
if(MK_BUILD_TESTS)
    list(APPEND COMPONENTS unity)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mkauto_dashboard)

if(NOT IDF_VERSION_MAJOR EQUAL 5 OR NOT IDF_VERSION_MINOR EQUAL 5 OR NOT IDF_VERSION_PATCH EQUAL 5)
    message(FATAL_ERROR "MKAuto requires ESP-IDF 5.5.5")
endif()
if(NOT IDF_TARGET STREQUAL "esp32p4")
    message(FATAL_ERROR "MKAuto requires target esp32p4")
endif()

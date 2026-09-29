cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(ENV_FILE "${CMAKE_SOURCE_DIR}/.env")

if(EXISTS "${ENV_FILE}")
    file(STRINGS "${ENV_FILE}" ENV_LINES)

    foreach(LINE IN LISTS ENV_LINES)
        if(LINE MATCHES "^([^#][^=]*)=(.*)$")
            set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
        endif()
    endforeach()
endif()

add_compile_definitions(
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
)

add_compile_options(-Wno-format)

project(watch)
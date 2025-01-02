cmake_minimum_required(VERSION 3.19)

# Audio Sending is implemented, but not performant enough yet
add_compile_definitions(SEND_AUDIO=0)

if(NOT DEFINED ENV{CI_BUILD})
  if(NOT IDF_TARGET STREQUAL linux)
    if(NOT DEFINED ENV{WIFI_SSID} OR NOT DEFINED ENV{WIFI_PASSWORD})
      message(FATAL_ERROR "Env variables WIFI_SSID and WIFI_PASSWORD must be set")
    endif()

    add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
    add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")
  endif()

  if(NOT DEFINED ENV{OPENAI_API_KEY})
    message(FATAL_ERROR "Env variable OPENAI_API_KEY must be set")
  endif()

  add_compile_definitions(OPENAI_API_KEY="$ENV{OPENAI_API_KEY}")
endif()

add_compile_definitions(OPENAI_REALTIMEAPI="https://api.openai.com/v1/realtime?model=gpt-4o-realtime-preview-2024-10-01")

set(COMPONENTS src)
set(EXTRA_COMPONENT_DIRS "src" "components/srtp" "components/peer" "components/esp-libopus")

if(IDF_TARGET STREQUAL linux)
	add_compile_definitions(LINUX_BUILD=1)
  list(APPEND EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/examples/protocols/linux_stubs/esp_stubs
    "components/esp-protocols/common_components/linux_compat/esp_timer"
    "components/esp-protocols/common_components/linux_compat/freertos"
    )
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(src)

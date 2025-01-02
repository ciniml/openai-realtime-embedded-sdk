set(COMMON_SRC "webrtc.cpp" "main.cpp" "http.cpp" "bsp.cpp")

if(IDF_TARGET STREQUAL linux)
	idf_component_register(
		SRCS ${COMMON_SRC}
		REQUIRES peer esp-libopus esp_http_client)
else()
	idf_component_register(
		SRCS ${COMMON_SRC} "wifi.cpp" "media.cpp"
		REQUIRES driver esp_wifi nvs_flash peer esp_psram esp-libopus esp_http_client esp_timer esp_driver_gpio wifi_provisioning esp_http_server mdns M5Unified
		EMBED_FILES index.html)
endif()

idf_component_get_property(lib peer COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=restrict)
target_compile_options(${lib} PRIVATE -Wno-error=stringop-truncation)

idf_component_get_property(lib srtp COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=incompatible-pointer-types)

idf_component_get_property(lib esp-libopus COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=maybe-uninitialized)
target_compile_options(${lib} PRIVATE -Wno-error=stringop-overread)

# These components are available in ESP-IDF >= 5.1
set(req esp_event)
if(NOT "${IDF_TARGET}" STREQUAL "linux")
    list(APPEND req esp_hw_support)
endif()
set(priv_req nvs_flash)

set(srcs "src/esp_diag_data_store.c")

if (CONFIG_DIAG_DATA_STORE_RTC OR CONFIG_DIAG_DATA_STORE_RAM)
    list(APPEND srcs "src/rtc_store/rtc_store.c")
    set(includes "src/rtc_store")
endif()

if(NOT "${IDF_TARGET}" STREQUAL "linux")
    list(APPEND priv_req app_update)
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ${includes} "include"
                       PRIV_REQUIRES ${priv_req}
                       REQUIRES ${req})

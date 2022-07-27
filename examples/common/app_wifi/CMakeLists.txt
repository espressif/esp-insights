# from IDF version 5.0, we need to explicitly specify requirements
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    set(priv_req esp_wifi)
endif()

idf_component_register(SRCS "app_wifi.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ${priv_req})

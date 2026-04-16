# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{INSIGHTS_PATH})
  set(INSIGHTS_PATH $ENV{INSIGHTS_PATH})
else()
  set(INSIGHTS_PATH ${CMAKE_CURRENT_LIST_DIR}/..)
endif(DEFINED ENV{INSIGHTS_PATH})
list(APPEND EXTRA_COMPONENT_DIRS ${INSIGHTS_PATH}/components)

if(NOT "${IDF_TARGET}" STREQUAL "linux")
  file(GLOB INSIGHTS_COMPONENTS "${INSIGHTS_PATH}/components/*")
else()
  # On Linux target, only include components that support it
  set(INSIGHTS_COMPONENTS "${INSIGHTS_PATH}/components/esp_diag_data_store")
endif()

foreach(COMPONENT ${INSIGHTS_COMPONENTS})
  get_filename_component(COMPONENT_NAME ${COMPONENT} NAME)
  if(EXISTS "${COMPONENT}/test/CMakeLists.txt")
    list(APPEND TEST_TARGETS ${COMPONENT_NAME})
    if(EXISTS "${COMPONENT}/test/sdkconfig.defaults")
      list(APPEND SDKCONFIG_FILES "${COMPONENT}/test/sdkconfig.defaults")
    endif()
  endif()
endforeach()

list(APPEND SDKCONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(SDKCONFIG_DEFAULTS ${SDKCONFIG_FILES})
set(TEST_COMPONENTS ${TEST_TARGETS})

if("${IDF_TARGET}" STREQUAL "linux")
  # Minimal component set for Linux target to avoid hardware HAL dependencies.
  # esp_log / esp_common are pulled in transitively via esp_event / nvs_flash.
  set(COMPONENTS main unity esp_event nvs_flash freertos esp_diag_data_store
                 esp_log esp_common)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_insights_test)

# TODO: drop once espressif/cbor managed component fixes its macro-redefinition
# warning on Linux (tracked upstream; file issue against espressif/cbor if not
# already open). Suppressing -Werror here as a targeted workaround.
if("${IDF_TARGET}" STREQUAL "linux")
  idf_component_get_property(cbor_lib espressif__cbor COMPONENT_LIB)
  if(cbor_lib)
    target_compile_options(${cbor_lib} PRIVATE -Wno-error)
  endif()
endif()

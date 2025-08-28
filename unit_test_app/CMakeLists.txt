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

file(GLOB INSIGHTS_COMPONENTS "${INSIGHTS_PATH}/components/*")

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

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_insights_test)

idf_build_get_property(python PYTHON)
idf_build_get_property(idf_path IDF_PATH)
idf_build_get_property(build_dir BUILD_DIR)

set(PROJ_BUILD_CONFIG_FILE project_build_config.json)
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-v${PROJECT_VER})

if(CONFIG_ESP_INSIGHTS_ENABLED)
# Capture script paths now since CMAKE_CURRENT_LIST_DIR won't resolve correctly in deferred context
set(ESP_INSIGHTS_SCRIPTS_DIR ${CMAKE_CURRENT_LIST_DIR}/scripts)

set(_insights_post_build_args
        TARGET app
        POST_BUILD
        COMMAND ${python} ${ESP_INSIGHTS_SCRIPTS_DIR}/get_projbuild_gitconfig.py ${PROJECT_DIR} ${CMAKE_PROJECT_NAME} ${PROJECT_VER} ${build_dir}/${PROJ_BUILD_CONFIG_FILE} ${idf_path} ${_CMAKE_TOOLCHAIN_PREFIX}
        COMMAND ${CMAKE_COMMAND}
        -D BUILD_DIR=${build_dir}
        -D PROJECT_DIR=${PROJECT_DIR}
        -D PROJECT_NAME=${CMAKE_PROJECT_NAME}
        -D PROJECT_VER=${PROJECT_VER}
        -D ARCHIVE_DIR=${ARCHIVE_NAME}
        -D PROJ_CONFIG_FILE=${PROJ_BUILD_CONFIG_FILE}
        -D PARTITION_CSV_FILE=${CONFIG_PARTITION_TABLE_CUSTOM_FILENAME}
        -P ${ESP_INSIGHTS_SCRIPTS_DIR}/gen_tar_dir.cmake
        COMMAND ${CMAKE_COMMAND} -E echo "===================== Generating insights firmware package build/${ARCHIVE_NAME}.zip ======================"
        COMMAND ${CMAKE_COMMAND} -E tar cfv ${ARCHIVE_NAME}.zip ${ARCHIVE_NAME} --format=zip
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${ARCHIVE_NAME}
        COMMAND ${CMAKE_COMMAND} -E remove ${PROJ_BUILD_CONFIG_FILE}
        VERBATIM
)

# cmake_language(DEFER) requires CMake >= 3.19. Use it when available to handle
# IDF v6.0+ where 'app' target doesn't exist when project_include.cmake runs.
# Fall back to direct add_custom_command for older CMake versions.
if(CMAKE_VERSION VERSION_LESS "3.19")
    add_custom_command(${_insights_post_build_args})
else()
    cmake_language(DEFER CALL add_custom_command ${_insights_post_build_args})
endif()

endif()

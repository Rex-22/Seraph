make_executable()

# Disable specific warnings for third-party STB library
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set_source_files_properties(ext/stb_image_write.cpp PROPERTIES
        COMPILE_FLAGS "-Wno-error=missing-field-initializers")
endif()

target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        ${SDL3_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${BGFX_INCLUDE_DIR}
        ${BX_INCLUDE_DIR}
        ${BIMG_INCLUDE_DIR}
        ${SPDLOG_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${JSON_INCLUDE_DIR}
        ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(${PROJECT}
        ${SDL3_LIBRARIES}
        ${IMGUI_LIBRARIES}
        ${BGFX_LIBRARIES}
        ${SPDLOG_LIBRARIES}
        ${GLM_LIBRARIES}
        ${JSON_LIBRARIES}
)
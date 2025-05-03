make_executable()

target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        ${SDL3_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${BGFX_INCLUDE_DIR}
        ${BX_INCLUDE_DIR}
        ${BIMG_INCLUDE_DIR}
        ${SPDLOG_INCLUDE_DIR}
        ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(${PROJECT}
        ${SDL3_LIBRARIES}
        ${IMGUI_LIBRARIES}
        ${BGFX_LIBRARIES}
        ${SPDLOG_LIBRARIES}
)
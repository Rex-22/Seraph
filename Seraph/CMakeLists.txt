set(PROJECT Seraph)

# EntryPoint.h / Seraph.h are client-facing headers: EntryPoint.h defines main()
# and must only be compiled into the consuming executable (Sandbox), never the
# library. They are headers, so the *.cpp glob never picks them up here.
make_library()

# Disable specific warnings for third-party STB library
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set_source_files_properties(src/ext/stb_image_write.cpp PROPERTIES
        COMPILE_FLAGS "-Wno-error=missing-field-initializers")
endif()

# Engine's own source root. PRIVATE (non-system) for our own build, so engine
# code keeps including engine headers with "". SYSTEM INTERFACE for consumers
# (Sandbox), so they include engine headers as <Seraph/...> and never inherit
# our warnings.
target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${PROJECT} SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Generated headers (config.h, etc.) — needed by both us and consumers.
target_include_directories(${PROJECT} PUBLIC
        ${PROJECT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_BINARY_DIR}/src
)

# Third-party headers as SYSTEM so their warnings are suppressed. SYSTEM also
# propagates to consumers (Sandbox), and avoids a -I/-isystem clash with the
# same dirs propagated by the vendor targets we link below.
target_include_directories(${PROJECT} SYSTEM PUBLIC
        ${SDL3_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${BGFX_INCLUDE_DIR}
        ${BX_INCLUDE_DIR}
        ${BIMG_INCLUDE_DIR}
        ${SPDLOG_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${JSON_INCLUDE_DIR}
        ${ENTT_INCLUDE_DIR}
        ${IMGUIZMO_INCLUDE_DIR}
)

# PUBLIC so the static library's dependencies propagate to consumers at link time.
target_link_libraries(${PROJECT} PUBLIC
        ${SDL3_LIBRARIES}
        ${IMGUI_LIBRARIES}
        ${BGFX_LIBRARIES}
        ${SPDLOG_LIBRARIES}
        ${GLM_LIBRARIES}
        ${JSON_LIBRARIES}
        ${ENTT_LIBRARIES}
        ${IMGUIZMO_LIBRARIES}
)
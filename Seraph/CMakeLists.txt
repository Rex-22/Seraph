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

# PUBLIC so consumers (Sandbox) inherit the engine + vendor include paths.
target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
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
        ${CMAKE_BINARY_DIR}/src
)

# PUBLIC so the static library's dependencies propagate to consumers at link time.
target_link_libraries(${PROJECT} PUBLIC
        ${SDL3_LIBRARIES}
        ${IMGUI_LIBRARIES}
        ${BGFX_LIBRARIES}
        ${SPDLOG_LIBRARIES}
        ${GLM_LIBRARIES}
        ${JSON_LIBRARIES}
)
set(PROJECT Seraph)

# EntryPoint.h / Seraph.h are client-facing headers: EntryPoint.h defines main()
# and must only be compiled into a consuming executable (Seraph-Editor /
# Seraph-Runtime), never the library. They are headers, so the *.cpp glob never
# picks them up here.
#
# SHARED so the editor and each project's loadable Game module link ONE libSeraph
# — giving a single ScriptRegistry/ShaderManager and consistent ScriptableEntity
# vtables across the dylib boundary.
make_library(SHARED)

# Windows: auto-export all symbols so the Game dylib resolves engine symbols
# without annotating every class with __declspec (no-op on macOS/Linux, where
# default visibility already exports). DEBUG_POSTFIX "" keeps the filename stable
# (libSeraph, never libSeraphd) so SeraphConfig's path and the baked @rpath
# references match the file in every build type — and packaging can copy it.
set_target_properties(Seraph PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    DEBUG_POSTFIX "")

# Disable specific warnings for third-party STB library
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set_source_files_properties(src/ext/stb_image_write.cpp PROPERTIES
        COMPILE_FLAGS "-Wno-error=missing-field-initializers")
endif()

# Engine's own source root. PRIVATE (non-system) for our own build, so engine
# code keeps including engine headers with "". SYSTEM INTERFACE for consumers
# (the client executables), so they include engine headers as <Seraph/...> and
# never inherit our warnings.
target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${PROJECT} SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Generated headers (config.h, etc.) — needed by both us and consumers.
target_include_directories(${PROJECT} PUBLIC
        ${PROJECT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_BINARY_DIR}/src
)

# Third-party headers as SYSTEM so their warnings are suppressed. SYSTEM also
# propagates to consumers (the client executables), and avoids a -I/-isystem
# clash with the same dirs propagated by the vendor targets we link below.
target_include_directories(${PROJECT} SYSTEM PUBLIC
        ${SDL3_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${BGFX_INCLUDE_DIR}
        ${BX_INCLUDE_DIR}
        ${BIMG_INCLUDE_DIR}
        ${SPDLOG_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${YAML_INCLUDE_DIR}
        ${ENTT_INCLUDE_DIR}
        ${IMGUIZMO_INCLUDE_DIR}
        ${ASSIMP_INCLUDE_DIR}
        ${JOLT_INCLUDE_DIR}
)

# PUBLIC so the static library's dependencies propagate to consumers at link time.
target_link_libraries(${PROJECT} PUBLIC
        ${SDL3_LIBRARIES}
        ${IMGUI_LIBRARIES}
        ${BGFX_LIBRARIES}
        ${SPDLOG_LIBRARIES}
        ${GLM_LIBRARIES}
        ${YAML_LIBRARIES}
        ${ENTT_LIBRARIES}
        ${IMGUIZMO_LIBRARIES}
        ${ASSIMP_LIBRARIES}
        ${JOLT_LIBRARIES}
)

# CoreServices provides FSEvents, the macOS backend for Platform/FileWatcher.
if (APPLE)
    target_link_libraries(${PROJECT} PUBLIC "-framework CoreServices")
endif ()

# Reflection code-gen. When SeraphHeaderTool is enabled, generate runtime
# reflection registrations from the SPROPERTY/SCLASS annotations in these headers
# (the generated .gen.cpp is compiled into libSeraph and self-registers at load).
# When OFF, these types simply aren't reflected — no libclang dependency. Types
# with hand-written registration (ScriptableEntity, MaterialParameterType) are
# NOT listed here: they demonstrate the hand-registration opt-out coexisting with
# generated types. See cmake/sht.cmake and Todo/plans/reflection-plan.md.
if (SERAPH_BUILD_HEADER_TOOL)
    include(${CMAKE_SOURCE_DIR}/cmake/sht.cmake)
    sht_reflect(${PROJECT} HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Seraph/Physics/PhysicsSettings.h)
endif ()
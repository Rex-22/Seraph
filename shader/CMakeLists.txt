if(NOT DEFINED SHADER_DIR)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

file(GLOB VERTEX_SHADERS "${SHADER_DIR}/vs_*.sc")
file(GLOB FRAGMENT_SHADERS "${SHADER_DIR}/fs_*.sc")
file(GLOB SERAPH_VARYING_DEF_FILE "${SHADER_DIR}/varying.def.sc")

file(GLOB CHUNK_VERTEX_SHADERS "${SHADER_DIR}/chunk/vs_*.sc")
file(GLOB CHUNK_FRAGMENT_SHADERS "${SHADER_DIR}/chunk/fs_*.sc")
file(GLOB CHUNK_SERAPH_VARYING_DEF_FILE "${SHADER_DIR}/chunk/varying.def.sc")

bgfx_compile_shaders(
        AS_HEADERS
        TYPE VERTEX
        INCLUDE_DIRS ${SHADER_DIR}
        SHADERS ${VERTEX_SHADERS}
        VARYING_DEF ${SERAPH_VARYING_DEF_FILE}
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
        OUT_FILES_VAR VERTEX_FILES
)

bgfx_compile_shaders(
        AS_HEADERS
        TYPE FRAGMENT
        INCLUDE_DIRS ${SHADER_DIR}
        SHADERS ${FRAGMENT_SHADERS}
        VARYING_DEF ${SERAPH_VARYING_DEF_FILE}
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
        OUT_FILES_VAR FRAGMENT_FILES
)

bgfx_compile_shaders(
        AS_HEADERS
        TYPE VERTEX
        INCLUDE_DIRS ${SHADER_DIR}
        SHADERS ${CHUNK_VERTEX_SHADERS}
        VARYING_DEF ${CHUNK_SERAPH_VARYING_DEF_FILE}
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
        OUT_FILES_VAR CHUNK_VERTEX_FILES
)

bgfx_compile_shaders(
        AS_HEADERS
        TYPE FRAGMENT
        INCLUDE_DIRS ${SHADER_DIR}
        SHADERS ${CHUNK_FRAGMENT_SHADERS}
        VARYING_DEF ${CHUNK_SERAPH_VARYING_DEF_FILE}
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
        OUT_FILES_VAR CHUNK_FRAGMENT_FILES
)

add_custom_target(
        Shaders ALL
        DEPENDS ${FRAGMENT_FILES} ${VERTEX_FILES}
        ${CHUNK_FRAGMENT_FILES} ${CHUNK_VERTEX_FILES}
)

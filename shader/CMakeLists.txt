if(NOT DEFINED SHADER_DIR)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

file(GLOB VERTEX_SHADERS "${SHADER_DIR}/v_*.sc")
file(GLOB FRAGMENT_SHADERS "${SHADER_DIR}/f_*.sc")
file(GLOB SERAPH_VARYING_DEF_FILE "${SHADER_DIR}/varying.def.sc")

bgfx_compile_shaders(
        AS_HEADERS
        TYPE VERTEX
        SHADERS ${VERTEX_SHADERS}
        VARYING_DEF ${SERAPH_VARYING_DEF_FILE}
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
        OUT_FILES_VAR VERTEX_FILES
)

bgfx_compile_shaders(
        AS_HEADERS
        TYPE FRAGMENT
        SHADERS ${FRAGMENT_SHADERS}
        VARYING_DEF ${SERAPH_VARYING_DEF_FILE}
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
        OUT_FILES_VAR FRAGMENT_FILES
)

add_custom_target(
        ${PROJECT} ALL
        DEPENDS ${FRAGMENT_FILES} ${VERTEX_FILES}
)

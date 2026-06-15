if(NOT DEFINED SHADER_DIR)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders)

file(GLOB_RECURSE SHADER_VARYING_DEFS CONFIGURE_DEPENDS "${SHADER_DIR}/varying.def.sc")

set(ALL_SHADER_OUTPUTS "")

foreach(VARYING_DEF ${SHADER_VARYING_DEFS})
    get_filename_component(SHADER_SET_DIR ${VARYING_DEF} DIRECTORY)

    file(GLOB SET_VERTEX_SHADERS CONFIGURE_DEPENDS "${SHADER_SET_DIR}/vs_*.sc")
    file(GLOB SET_FRAGMENT_SHADERS CONFIGURE_DEPENDS "${SHADER_SET_DIR}/fs_*.sc")

    if(SET_VERTEX_SHADERS)
        bgfx_compile_shaders(
                AS_HEADERS
                TYPE VERTEX
                INCLUDE_DIRS ${SHADER_DIR} ${SHADER_SET_DIR}
                SHADERS ${SET_VERTEX_SHADERS}
                VARYING_DEF ${VARYING_DEF}
                OUTPUT_DIR ${SHADER_OUTPUT_DIR}
                OUT_FILES_VAR SET_VERTEX_OUTPUTS
        )
        list(APPEND ALL_SHADER_OUTPUTS ${SET_VERTEX_OUTPUTS})
    endif ()

    if(SET_FRAGMENT_SHADERS)
        bgfx_compile_shaders(
                AS_HEADERS
                TYPE FRAGMENT
                INCLUDE_DIRS ${SHADER_DIR} ${SHADER_SET_DIR}
                SHADERS ${SET_FRAGMENT_SHADERS}
                VARYING_DEF ${VARYING_DEF}
                OUTPUT_DIR ${SHADER_OUTPUT_DIR}
                OUT_FILES_VAR SET_FRAGMENT_OUTPUTS
        )
        list(APPEND ALL_SHADER_OUTPUTS ${SET_FRAGMENT_OUTPUTS})
    endif ()
endforeach ()

add_custom_target(
        Shaders ALL
        DEPENDS ${ALL_SHADER_OUTPUTS}
)

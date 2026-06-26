if(NOT DEFINED SHADER_DIR)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders)

file(GLOB_RECURSE SHADER_VARYING_DEFS CONFIGURE_DEPENDS "${SHADER_DIR}/varying.def.sc")

set(ALL_SHADER_OUTPUTS "")

# Fragments of the generated shader registry (see ShaderRegistry.cpp.in),
# accumulated as we walk each shader set below.
set(SERAPH_SHADER_INCLUDES "")
set(SERAPH_SHADER_EMBEDDED_ENTRIES "")
set(SERAPH_SHADER_REGISTRATIONS "")

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

    # Embed every compiled shader in this set into the generated registry.
    foreach(SHADER ${SET_VERTEX_SHADERS} ${SET_FRAGMENT_SHADERS})
        get_filename_component(SHADER_BASENAME ${SHADER} NAME_WE)
        string(APPEND SERAPH_SHADER_INCLUDES
                "#define SHADER_NAME ${SHADER_BASENAME}\n#include \"Seraph/Graphics/ShaderIncluder.h\"\n")
        string(APPEND SERAPH_SHADER_EMBEDDED_ENTRIES
                "    BGFX_EMBEDDED_SHADER(${SHADER_BASENAME}),\n")
    endforeach ()

    # Pair vs_<name>/fs_<name> into a program registered under <name>.
    foreach(VERTEX_SHADER ${SET_VERTEX_SHADERS})
        get_filename_component(VERTEX_NAME ${VERTEX_SHADER} NAME_WE)   # vs_simple
        string(REGEX REPLACE "^vs_" "" PROGRAM_NAME ${VERTEX_NAME})    # simple
        set(FRAGMENT_NAME "fs_${PROGRAM_NAME}")
        if(EXISTS "${SHADER_SET_DIR}/${FRAGMENT_NAME}.sc")
            string(APPEND SERAPH_SHADER_REGISTRATIONS
                    "    Seraph::ShaderManager::RegisterEmbedded(\"${PROGRAM_NAME}\", s_embeddedShaders, \"${VERTEX_NAME}\", \"${FRAGMENT_NAME}\");\n")
        else ()
            message(WARNING
                    "Shader '${VERTEX_NAME}' has no matching '${FRAGMENT_NAME}.sc'; no program registered for it.")
        endif ()
    endforeach ()
endforeach ()

add_custom_target(
        Shaders ALL
        DEPENDS ${ALL_SHADER_OUTPUTS}
)

# Generate the shader registry translation unit.
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/ShaderRegistry.cpp.in
        ${CMAKE_BINARY_DIR}/generated/ShaderRegistry.generated.cpp
        @ONLY
)

set(SERAPH_SHADER_REGISTRY_SRC
        ${CMAKE_BINARY_DIR}/generated/ShaderRegistry.generated.cpp
        CACHE INTERNAL "Generated Seraph shader registry source")

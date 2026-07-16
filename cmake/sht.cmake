# SeraphHeaderTool (SHT) integration.
#
#   sht_reflect(<target> HEADERS <h1> [<h2> ...])
#
# For each header, generate <target-binary-dir>/gen/<name>.gen.cpp via
# SeraphHeaderTool and add it to <target>'s sources. The generated file contains
# the fluent SP_REFLECT_* registrations for the annotated types in that header
# (see Tools/SeraphHeaderTool). Regeneration tracks the header's transitive
# includes via a DEPFILE, so editing any included header regenerates correctly.
#
# The tool is resolved as, in order:
#   1. the in-tree `SeraphHeaderTool` target (built when SERAPH_BUILD_HEADER_TOOL=ON), or
#   2. the cache path SERAPH_HEADER_TOOL_EXE (a prebuilt tool).
#
# libclang is a raw frontend and does NOT auto-add the platform SDK sysroot the
# clang++ driver would, so on Apple we pass -isysroot explicitly (SHT 2 finding).
# The parse uses the target's own INCLUDE_DIRECTORIES and COMPILE_DEFINITIONS so
# headers that pull in engine/vendor headers resolve exactly as in a real compile.

function(sht_reflect target)
    cmake_parse_arguments(SHT "" "" "HEADERS" ${ARGN})

    if(TARGET SeraphHeaderTool)
        set(_sht_exe "$<TARGET_FILE:SeraphHeaderTool>")
        set(_sht_dep SeraphHeaderTool)
    elseif(SERAPH_HEADER_TOOL_EXE)
        set(_sht_exe "${SERAPH_HEADER_TOOL_EXE}")
        set(_sht_dep "${SERAPH_HEADER_TOOL_EXE}")
    else()
        message(FATAL_ERROR
            "sht_reflect(${target}): no SeraphHeaderTool. Enable "
            "-DSERAPH_BUILD_HEADER_TOOL=ON or set -DSERAPH_HEADER_TOOL_EXE=<path>.")
    endif()

    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/gen")
    file(MAKE_DIRECTORY "${_gen_dir}")

    # SDK sysroot for the libclang parse (Apple).
    set(_sysroot "")
    if(APPLE)
        execute_process(COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _sdk OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_sdk)
            set(_sysroot -isysroot "${_sdk}")
        endif()
    endif()

    # Target include dirs + defs as -I / -D flags (generator expressions so they
    # reflect the fully-resolved set). COMMAND_EXPAND_LISTS splits the ';' lists.
    set(_inc "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>")
    set(_def "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
    set(_inc_flags "$<$<BOOL:${_inc}>:-I$<JOIN:${_inc},;-I>>")
    set(_def_flags "$<$<BOOL:${_def}>:-D$<JOIN:${_def},;-D>>")

    set(_gen_sources "")
    foreach(_hdr ${SHT_HEADERS})
        get_filename_component(_hdr_abs "${_hdr}" ABSOLUTE)
        get_filename_component(_name "${_hdr}" NAME_WE)
        set(_out "${_gen_dir}/${_name}.gen.cpp")
        set(_dep "${_gen_dir}/${_name}.gen.d")

        add_custom_command(
            OUTPUT "${_out}"
            COMMAND "${_sht_exe}" "${_hdr_abs}"
                    -o "${_out}" --include "${_hdr_abs}" --depfile "${_dep}"
                    -- ${_sysroot} -std=c++20 "${_inc_flags}" "${_def_flags}"
            DEPENDS "${_hdr_abs}" "${_sht_dep}"
            DEPFILE "${_dep}"
            COMMAND_EXPAND_LISTS
            VERBATIM
            COMMENT "SHT reflect ${_name}")

        list(APPEND _gen_sources "${_out}")
    endforeach()

    target_sources(${target} PRIVATE ${_gen_sources})
endfunction()

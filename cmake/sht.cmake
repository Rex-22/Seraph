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

    # Downstream (find_package) projects receive the engine's headers and ABI
    # defs through the IMPORTED Seraph::Seraph target's USAGE REQUIREMENTS, which
    # are applied to the compile line but do NOT land in the consuming target's
    # own INCLUDE_DIRECTORIES/COMPILE_DEFINITIONS. Pass those interface sets too so
    # the libclang parse resolves engine headers exactly as the real compile does.
    # Empty (and therefore dropped by COMMAND_EXPAND_LISTS) in the engine's own
    # build, where Seraph::Seraph doesn't exist and the target already carries its
    # own include dirs.
    set(_iface_inc_flags "")
    set(_iface_def_flags "")
    if(TARGET Seraph::Seraph)
        set(_iinc "$<TARGET_PROPERTY:Seraph::Seraph,INTERFACE_INCLUDE_DIRECTORIES>")
        set(_idef "$<TARGET_PROPERTY:Seraph::Seraph,INTERFACE_COMPILE_DEFINITIONS>")
        set(_iface_inc_flags "$<$<BOOL:${_iinc}>:-I$<JOIN:${_iinc},;-I>>")
        set(_iface_def_flags "$<$<BOOL:${_idef}>:-D$<JOIN:${_idef},;-D>>")
    endif()

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
                    -- ${_sysroot} -std=c++20
                    "${_inc_flags}" "${_def_flags}"
                    "${_iface_inc_flags}" "${_iface_def_flags}"
            DEPENDS "${_hdr_abs}" "${_sht_dep}"
            DEPFILE "${_dep}"
            COMMAND_EXPAND_LISTS
            VERBATIM
            COMMENT "SHT reflect ${_name}")

        list(APPEND _gen_sources "${_out}")
    endforeach()

    target_sources(${target} PRIVATE ${_gen_sources})
endfunction()

# sht_reflect_glob(<target> DIRS <dir> [<dir> ...])
#
# Auto-discover annotated headers instead of listing them by hand: recursively
# glob *.h under each DIR, keep only headers that actually contain an SHT
# annotation macro (a cheap configure-time content scan), and hand the survivors
# to sht_reflect. This removes the "annotated a header but forgot to add it to
# the HEADERS list -> silently not reflected" footgun.
#
# Reconfigure semantics: CONFIGURE_DEPENDS re-globs on build so a brand-new file
# is noticed, but the content filter runs at CONFIGURE time — so ADDING an
# annotation to an existing header needs a reconfigure to be picked up (same cost
# as adding any new source). Annotations.h (which DEFINES the macros) is excluded.
function(sht_reflect_glob target)
    cmake_parse_arguments(SHTG "" "" "DIRS" ${ARGN})

    set(_annotated "")
    foreach(_dir ${SHTG_DIRS})
        file(GLOB_RECURSE _candidates CONFIGURE_DEPENDS "${_dir}/*.h")
        foreach(_h ${_candidates})
            get_filename_component(_name "${_h}" NAME)
            if(_name STREQUAL "Annotations.h")
                continue() # defines the macros; never a reflection subject
            endif()
            # Match a macro USE: the macro name followed by '(' but NOT preceded
            # by '#define '. file(STRINGS REGEX) is line-based; exclude #define
            # lines by requiring no '#' before the macro on the line.
            file(STRINGS "${_h}" _hit
                REGEX "^[^#]*S(CLASS|PROPERTY|ENUM|FUNCTION)[ \t]*\\("
                LIMIT_COUNT 1)
            if(_hit)
                list(APPEND _annotated "${_h}")
            endif()
        endforeach()
    endforeach()

    list(LENGTH _annotated _count)
    message(STATUS "SHT: ${_count} annotated header(s) auto-discovered for ${target}")
    if(_annotated)
        sht_reflect(${target} HEADERS ${_annotated})
    endif()
endfunction()

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

    # Clang resource dir for the libclang parse (non-Apple). libclang is a raw
    # frontend and — unlike the clang++ driver — does NOT add its own builtin
    # headers (stddef.h, stdarg.h, ...) to the include path. Without -resource-dir,
    # parsing any header that pulls in <cstdint>/<cstddef> fails with
    # "stddef.h file not found". On Apple the -isysroot above already covers this
    # (and the macOS path is verified), so we only add it elsewhere to avoid
    # overriding the toolchain's own resource dir. Resolved once (cached).
    set(_resource "")
    if(NOT APPLE)
        find_program(SHT_CLANG_EXE
            NAMES clang clang-19 clang-18 clang-17
            DOC "clang binary used to locate the libclang resource dir for SHT")
        set(_resdir "")
        if(SHT_CLANG_EXE)
            execute_process(COMMAND "${SHT_CLANG_EXE}" -print-resource-dir
                OUTPUT_VARIABLE _resdir OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
        endif()
        # Fallback: derive from the libclang library location
        # (<libdir>/clang/<ver> or <libdir>/../lib/clang/<ver>).
        if((NOT _resdir OR NOT EXISTS "${_resdir}/include") AND SERAPH_LIBCLANG_LIBRARY)
            get_filename_component(_lcdir "${SERAPH_LIBCLANG_LIBRARY}" DIRECTORY)
            file(GLOB _res_incs
                "${_lcdir}/clang/*/include" "${_lcdir}/../lib/clang/*/include")
            if(_res_incs)
                list(SORT _res_incs)
                list(GET _res_incs -1 _res_inc) # highest version
                get_filename_component(_resdir "${_res_inc}" DIRECTORY)
            endif()
        endif()
        if(_resdir AND EXISTS "${_resdir}/include")
            set(_resource -resource-dir "${_resdir}")
        else()
            message(WARNING
                "sht_reflect(${target}): could not locate a clang resource dir; "
                "libclang may fail to find builtin headers (stddef.h). Install "
                "clang or pass -DSHT_CLANG_EXE=<path to clang>.")
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
                    -- ${_sysroot} ${_resource} -std=c++20
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
# Wire a reflection gen step for EVERY header under each DIR — not just the ones
# that happen to be annotated at configure time. The tool itself decides per
# header at BUILD time whether there is anything to reflect: a header with no
# SCLASS/SPROPERTY/SENUM macro use is fast-skipped (a cheap text scan, no libclang)
# and emits an empty .gen.cpp; only annotated headers pay the libclang parse cost.
#
# Why this shape: CMake fixes a target's source list at CONFIGURE time. If the
# header set were filtered by content at configure time (the old behavior), then
# ADDING an annotation to an existing header would need a manual reconfigure to be
# picked up. By giving every header a gen step, adding an annotation is picked up
# by a plain BUILD (the header changed -> its gen command reruns -> the tool now
# sees the annotation and emits registrations). No command, no reconfigure.
#
# Performance: the per-header text pre-scan is negligible; empty .gen.cpp stubs
# compile in milliseconds; only genuinely annotated headers invoke libclang, and
# the DEPFILE means a shared-header edit only reruns the gen steps that include it.
#
# CONFIGURE_DEPENDS still re-globs on build so a brand-NEW file (one that did not
# exist at configure) triggers a reconfigure — unavoidable, since CMake cannot add
# a source it has never seen. Annotations.h (which DEFINES the macros) is excluded.
function(sht_reflect_glob target)
    cmake_parse_arguments(SHTG "" "" "DIRS" ${ARGN})

    set(_headers "")
    foreach(_dir ${SHTG_DIRS})
        file(GLOB_RECURSE _candidates CONFIGURE_DEPENDS "${_dir}/*.h")
        foreach(_h ${_candidates})
            get_filename_component(_name "${_h}" NAME)
            if(_name STREQUAL "Annotations.h")
                continue() # defines the macros; never a reflection subject
            endif()
            list(APPEND _headers "${_h}")
        endforeach()
    endforeach()

    list(LENGTH _headers _count)
    message(STATUS "SHT: ${_count} header(s) wired for reflection codegen for ${target}")
    if(_headers)
        sht_reflect(${target} HEADERS ${_headers})
    endif()
endfunction()

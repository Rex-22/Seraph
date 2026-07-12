# Assimp model-importer configuration (mesh loading).
#
# Configured lean: no tests, tools, or exporters, and only the OBJ + glTF
# importers are compiled in — everything else off to keep build time and
# binary size down. Fetched like the other non-submodule dependencies.

include(FetchContent)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v6.0.5
    GIT_SHALLOW TRUE
    SYSTEM
)

set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE INTERNAL "")
set(ASSIMP_INSTALL OFF CACHE INTERNAL "")
set(ASSIMP_INSTALL_PDB OFF CACHE INTERNAL "")
set(ASSIMP_NO_EXPORT ON CACHE INTERNAL "")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_DRACO OFF CACHE INTERNAL "")

# Only build the importers we actually use.
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE INTERNAL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE INTERNAL "")
# glTF's bundled data can be zlib-compressed. Use the system zlib rather than
# assimp's vendored copy, whose zutil.h redefines fdopen and clashes with the
# macOS <stdio.h> declaration (build error). macOS/Linux ship zlib.
set(ASSIMP_BUILD_ZLIB OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(assimp)

# Source headers + the generated config/revision headers in the binary dir.
set(ASSIMP_INCLUDE_DIR
    ${assimp_SOURCE_DIR}/include
    ${assimp_BINARY_DIR}/include
    CACHE PATH "Assimp include directory")
set(ASSIMP_LIBRARIES assimp::assimp CACHE STRING "Assimp library")

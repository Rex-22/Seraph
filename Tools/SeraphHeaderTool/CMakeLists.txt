# SeraphHeaderTool (SHT) — host code-gen tool for runtime reflection.
#
# Builds standalone (cmake -S Tools/SeraphHeaderTool -B <build>) OR is pulled in
# by the root build via add_subdirectory when SERAPH_BUILD_HEADER_TOOL is ON. It
# links libclang (C API) and does NOT link libSeraph — it only PARSES engine
# headers, it is not part of the engine.
#
# libclang acquisition, in order:
#   1. Explicit  -DSERAPH_LIBCLANG_LIBRARY=<path to lib>  (and optional
#      -DSERAPH_LIBCLANG_INCLUDE_DIR to use a matching system clang-c/).
#   2. Homebrew / system LLVM (LIBCLANG_PATH env, brew llvm prefix, distro dirs).
#   3. Apple toolchain fallback (Xcode / CommandLineTools) on macOS.
# Headers: we VENDOR a pinned clang-c/ (vendor/clang-c, LLVM 17.0.6) so the build
# never requires a matching system clang-c to be installed. The libclang C ABI is
# stable across versions, so a newer/older system libclang links fine against the
# pinned headers.

cmake_minimum_required(VERSION 3.30)
project(SeraphHeaderTool CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED SERAPH_LIBCLANG_LIBRARY OR SERAPH_LIBCLANG_LIBRARY STREQUAL "")
    find_library(SERAPH_LIBCLANG_LIBRARY
        NAMES clang libclang
        HINTS
            ENV LIBCLANG_PATH
            /opt/homebrew/opt/llvm/lib
            /usr/local/opt/llvm/lib
            /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib
            /Library/Developer/CommandLineTools/usr/lib
            /usr/lib/llvm-22/lib
            /usr/lib/llvm-21/lib
            /usr/lib/llvm-20/lib
            /usr/lib/llvm-19/lib
            /usr/lib/llvm-18/lib
            /usr/lib/llvm-17/lib
            /usr/lib
        DOC "libclang C-API shared library for SeraphHeaderTool")
endif()

if(NOT SERAPH_LIBCLANG_LIBRARY)
    message(FATAL_ERROR
        "SeraphHeaderTool: libclang not found. Install LLVM (brew install llvm / "
        "apt install libclang-dev) or pass "
        "-DSERAPH_LIBCLANG_LIBRARY=/path/to/libclang.{dylib,so,lib}.")
endif()

add_executable(SeraphHeaderTool src/main.cpp)

target_include_directories(SeraphHeaderTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor)   # pinned clang-c/ headers
if(SERAPH_LIBCLANG_INCLUDE_DIR)
    target_include_directories(SeraphHeaderTool BEFORE PRIVATE
        ${SERAPH_LIBCLANG_INCLUDE_DIR})   # optional: prefer matching system headers
endif()

target_link_libraries(SeraphHeaderTool PRIVATE ${SERAPH_LIBCLANG_LIBRARY})

message(STATUS "SeraphHeaderTool: libclang = ${SERAPH_LIBCLANG_LIBRARY}")

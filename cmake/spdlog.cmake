set(SPDLOG_BUILD_EXAMPLE_HO FALSE)

add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/spdlog)

set(SPDLOG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/spdlog/include)

set(SPDLOG_LIBRARIES spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

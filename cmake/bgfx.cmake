set(BGFX_BUILD_EXAMPLE_COMMON OFF)
set(BGFX_BUILD_EXAMPLES OFF)

set(BGFX_BUILD_TOOLS ON)
set(BGFX_BUILD_TOOLS_SHADER ON)

add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/bgfx EXCLUDE_FROM_ALL)

set(BGFX_INCLUDE_DIR ${BGFX_DIR}/include)
set(BX_INCLUDE_DIR ${BX_DIR}/include)
set(BIMG_INCLUDE_DIR ${BIMG_DIR}/include)

set(BGFX_LIBRARIES bgfx bx bimg bimg_decode bimg_encode)

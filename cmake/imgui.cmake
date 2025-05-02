set(IMGUI_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
file(GLOB IMGUI_SOURCES ${IMGUI_INCLUDE_DIR}/*.cpp)
file(GLOB IMGUI_HEADERS ${IMGUI_INCLUDE_DIR}/*.h)

set(IMGUI_BACKENDS_DIR ${IMGUI_INCLUDE_DIR}/backends)
set(IMGUI_BACKEND_SOURCES
        ${IMGUI_BACKENDS_DIR}/imgui_impl_sdl3.cpp)
set(IMGUI_BACKEND_HEADERS
        ${IMGUI_BACKENDS_DIR}/imgui_impl_sdl3.h)

add_library(imgui STATIC
        ${IMGUI_SOURCES} ${IMGUI_SOURCES}
        ${IMGUI_BACKEND_HEADERS} ${IMGUI_BACKEND_SOURCES}
)

target_include_directories(imgui PUBLIC
        ${SDL3_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${IMGUI_BACKENDS_DIR}
        ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(imgui ${SDL3_LIBRARIES})

set_target_properties(imgui PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(imgui PROPERTIES FOLDER vendor)

set(IMGUI_LIBRARIES imgui)

include(FetchContent)

FetchContent_Declare(
    ImGuizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        1.10
)

FetchContent_MakeAvailable(ImGuizmo)

target_link_libraries(imguizmo PUBLIC ${IMGUI_LIBRARIES})

set(IMGUIZMO_INCLUDE_DIR ${imguizmo_SOURCE_DIR}/src)
set(IMGUIZMO_LIBRARIES imguizmo)

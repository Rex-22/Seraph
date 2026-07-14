# Some fetched dependencies (yaml-cpp, assimp's bundled contrib) still declare a
# cmake_minimum_required below what current CMake accepts. Raise the policy floor
# so those subprojects configure without editing their CMakeLists.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(cmake/sdl.cmake)
include(cmake/imgui.cmake)
include(cmake/bgfx.cmake)
include(cmake/shaders.cmake)
include(cmake/spdlog.cmake)
include(cmake/glm.cmake)
include(cmake/yaml.cmake)
include(cmake/entt.cmake)
include(cmake/imguizmo.cmake)
include(cmake/assimp.cmake)
include(cmake/jolt.cmake)
# yaml-cpp library configuration.
#
# Used for the engine's text serialization: the asset registry today, and
# scene/material text files later. Fetched (not vendored) like nlohmann/json
# and entt so no submodule is required.

include(FetchContent)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
    GIT_SHALLOW TRUE
    SYSTEM
)

# Lean static build — no tests, tools, or contrib add-ons.
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_TOOLS OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE INTERNAL "")
set(YAML_CPP_INSTALL OFF CACHE INTERNAL "")
set(YAML_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(yaml-cpp)

set(YAML_INCLUDE_DIR ${yaml-cpp_SOURCE_DIR}/include CACHE PATH "yaml-cpp include directory")
set(YAML_LIBRARIES yaml-cpp::yaml-cpp CACHE STRING "yaml-cpp library")

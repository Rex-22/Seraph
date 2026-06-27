include(FetchContent)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.16.0
    GIT_SHALLOW TRUE
    SYSTEM
)

set(ENTT_BUILD_TESTING OFF CACHE INTERNAL "")
set(ENTT_INSTALL OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(entt)

set(ENTT_INCLUDE_DIR ${entt_SOURCE_DIR}/src CACHE PATH "EnTT include directory")
set(ENTT_LIBRARIES EnTT::EnTT CACHE STRING "EnTT library")
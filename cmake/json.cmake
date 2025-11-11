# nlohmann/json library configuration

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# Configure nlohmann/json options
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(nlohmann_json)

# Set variables for the main project
set(JSON_INCLUDE_DIR ${nlohmann_json_SOURCE_DIR}/include CACHE PATH "nlohmann/json include directory")
set(JSON_LIBRARIES nlohmann_json::nlohmann_json CACHE STRING "nlohmann/json library")

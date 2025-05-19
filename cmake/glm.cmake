add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/glm)

add_compile_definitions("GLM_ENABLE_EXPERIMENTAL")
set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/spdlog/include)

set(GLM_LIBRARIES glm::glm)

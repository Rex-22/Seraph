add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/glm SYSTEM)

add_compile_definitions("GLM_ENABLE_EXPERIMENTAL")
add_compile_definitions("GLM_FORCE_DEPTH_ZERO_TO_ONE")
set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/glm)

set(GLM_LIBRARIES glm::glm)

set(PROJECT Sandbox)

make_executable()

# Link the engine. Seraph propagates its PUBLIC include dirs and vendor
# libraries transitively, so Sandbox does not repeat them.
target_link_libraries(${PROJECT} PRIVATE Seraph)

# ExampleLayer embeds the compiled bgfx shaders (generated headers), so the
# Shaders target must run before Sandbox is compiled.
add_dependencies(${PROJECT} Shaders)
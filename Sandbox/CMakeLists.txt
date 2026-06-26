set(PROJECT Sandbox)

make_executable()

# Link the engine. Seraph propagates its PUBLIC include dirs and vendor
# libraries transitively, so Sandbox does not repeat them.
target_link_libraries(${PROJECT} PRIVATE Seraph)

# ExampleLayer embeds the compiled bgfx shaders (generated headers), so the
# Shaders target must run before Sandbox is compiled.
add_dependencies(${PROJECT} Shaders)

# Compile the generated shader registry into Sandbox. It embeds the compiled
# shader bytes and registers them with Seraph::ShaderManager so client code can
# fetch programs by name.
target_sources(${PROJECT} PRIVATE ${SERAPH_SHADER_REGISTRY_SRC})
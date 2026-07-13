set(PROJECT Seraph-Runtime)

make_executable()

# Link the engine. Seraph propagates its PUBLIC include dirs and vendor
# libraries transitively, so the runtime does not repeat them.
target_link_libraries(${PROJECT} PRIVATE Seraph)

# The default material embeds the compiled bgfx shaders (generated headers), so
# the Shaders target must run before the runtime is compiled.
add_dependencies(${PROJECT} Shaders)

# Compile the generated shader registry into the runtime. It embeds the compiled
# shader bytes and registers them with Seraph::ShaderManager — the packed scene's
# meshes resolve their default material against these embedded shaders.
target_sources(${PROJECT} PRIVATE ${SERAPH_SHADER_REGISTRY_SRC})

set(PROJECT Seraph-Runtime)

make_executable()

# Link the engine + this project's Game module (native C++ scripts). Seraph
# propagates its PUBLIC include dirs and vendor libraries transitively, so the
# runtime does not repeat them. Game is an OBJECT library, so its self-registering
# scripts link directly into the runtime and appear in ScriptRegistry.
target_link_libraries(${PROJECT} PRIVATE Seraph Game)

# The default material embeds the compiled bgfx shaders (generated headers), so
# the Shaders target must run before the runtime is compiled.
add_dependencies(${PROJECT} Shaders)

# Compile the generated shader registry into the runtime. It embeds the compiled
# shader bytes and registers them with Seraph::ShaderManager — the packed scene's
# meshes resolve their default material against these embedded shaders.
target_sources(${PROJECT} PRIVATE ${SERAPH_SHADER_REGISTRY_SRC})

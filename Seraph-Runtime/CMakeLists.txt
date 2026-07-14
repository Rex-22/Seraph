set(PROJECT Seraph-Runtime)

make_executable()

# Link the shared engine. The project's Game module is NOT linked — the runtime
# loads it at startup (ScriptLibrary / SDL_LoadObject). Ship libGame + libSeraph
# alongside the runtime.
target_link_libraries(${PROJECT} PRIVATE Seraph)

# libSeraph is a shared library co-located in bin/; resolve it at runtime via a
# relative rpath (macOS @executable_path / Linux $ORIGIN; Windows: adjacent DLL).
set_target_properties(${PROJECT} PROPERTIES
    BUILD_RPATH "${SP_EXECUTABLE_RPATH}"
    INSTALL_RPATH "${SP_EXECUTABLE_RPATH}")

# The default material embeds the compiled bgfx shaders (generated headers), so
# the Shaders target must run before the runtime is compiled.
add_dependencies(${PROJECT} Shaders)

# Compile the generated shader registry into the runtime. It embeds the compiled
# shader bytes and registers them with Seraph::ShaderManager — the packed scene's
# meshes resolve their default material against these embedded shaders.
target_sources(${PROJECT} PRIVATE ${SERAPH_SHADER_REGISTRY_SRC})

set(PROJECT Seraph-Editor)

make_executable()

# Link the shared engine. The project's Game module is NOT linked — the editor
# loads it at runtime (ScriptLibrary / SDL_LoadObject) so it stays portable
# across projects with no recompile.
target_link_libraries(${PROJECT} PRIVATE Seraph)

# libSeraph is a shared library co-located in bin/; resolve it at runtime via a
# relative rpath (macOS @executable_path / Linux $ORIGIN; Windows: adjacent DLL).
set_target_properties(${PROJECT} PROPERTIES
    BUILD_RPATH "${SP_EXECUTABLE_RPATH}"
    INSTALL_RPATH "${SP_EXECUTABLE_RPATH}")

# The default material embeds the compiled bgfx shaders (generated headers), so
# the Shaders target must run before the editor is compiled.
add_dependencies(${PROJECT} Shaders)

# Compile the generated shader registry into the editor. It embeds the compiled
# shader bytes and registers them with Seraph::ShaderManager so client code can
# fetch programs by name.
target_sources(${PROJECT} PRIVATE ${SERAPH_SHADER_REGISTRY_SRC})

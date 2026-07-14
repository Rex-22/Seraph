# JoltPhysics — vendored as a submodule under vendor/JoltPhysics (pinned v5.6.0).
# Its CMakeLists lives in Build/ and creates the `Jolt` target, which propagates
# its JPH_* compile definitions (JPH_DEBUG_RENDERER / JPH_OBJECT_LAYER_BITS /
# JPH_ENABLE_ASSERTS / ...) and its include dir as PUBLIC to consumers that link
# it (see Jolt/Jolt.cmake: target_compile_definitions(Jolt PUBLIC ...) +
# target_include_directories(Jolt PUBLIC ...)). Those defines change sizeof() of
# Jolt structs, so the ONLY safe way to stay ABI-consistent is to LINK the target
# and NOT redefine JPH_* ourselves. All toggles are set (FORCE) before
# add_subdirectory and are identical across Debug/Release.

# --- Build hygiene when included as a subproject -----------------------------
# OVERRIDE_CXX_FLAGS (default ON) rewrites CMAKE_CXX_FLAGS_DEBUG/RELEASE — leave
# the platform/parent flags alone. INTERPROCEDURAL_OPTIMIZATION (default ON)
# makes libJolt.a LTO bitcode; linking it from our non-LTO targets fails with
# "file format not recognized", so it must be OFF.
set(OVERRIDE_CXX_FLAGS               OFF CACHE BOOL   "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION     OFF CACHE BOOL   "" FORCE)

# --- ABI / feature toggles (identical Debug & Release) -----------------------
set(DOUBLE_PRECISION                 OFF CACHE BOOL   "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC     OFF CACHE BOOL   "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL  "" FORCE)  # FP traps clash on macOS
set(CPP_EXCEPTIONS_ENABLED           ON  CACHE BOOL   "" FORCE)  # match the engine (uses exceptions)
set(CPP_RTTI_ENABLED                 ON  CACHE BOOL   "" FORCE)  # match the engine's RTTI
set(ENABLE_OBJECT_STREAM             OFF CACHE BOOL   "" FORCE)  # we serialize via YAML
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON CACHE BOOL "" FORCE)  # needed by the debug-draw bridge (Physics 9)
set(PROFILER_IN_DEBUG_AND_RELEASE    OFF CACHE BOOL   "" FORCE)
set(USE_ASSERTS                      OFF CACHE BOOL   "" FORCE)
set(OBJECT_LAYER_BITS                16  CACHE STRING "" FORCE)  # LayerID doubles as broadphase layer (<256)

# --- v5.6.0 GPU compute / hair-sim backends: not used, and MTL would drag in a
#     Metal dependency on macOS. Keep the core lib lean (rigid bodies only).
set(JPH_USE_DX12                     OFF CACHE BOOL   "" FORCE)
set(JPH_USE_VK                       OFF CACHE BOOL   "" FORCE)
set(JPH_USE_MTL                      OFF CACHE BOOL   "" FORCE)
set(JPH_USE_CPU_COMPUTE              OFF CACHE BOOL   "" FORCE)

# X86 ISA options are guarded by CMAKE_SYSTEM_PROCESSOR MATCHES "^x86" inside
# Jolt, so on Apple Silicon (arm64) they are ignored and NEON is used. Leave them.

# SYSTEM matches glm/bgfx so Jolt's headers don't inherit our warnings.
# EXCLUDE_FROM_ALL: Jolt only builds because the engine links it (like bgfx).
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/JoltPhysics/Build
                 ${CMAKE_BINARY_DIR}/vendor/JoltPhysics EXCLUDE_FROM_ALL SYSTEM)

# Repo convention. Real include/define propagation comes from linking `Jolt`.
set(JOLT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/JoltPhysics CACHE PATH   "Jolt include directory")
set(JOLT_LIBRARIES   Jolt                                   CACHE STRING "Jolt library")

cmake_minimum_required(VERSION 3.20)
project(DyingLightHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "DyingLightHeadTracking must be built as x64. Configure with -A x64.")
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Shared CameraUnlock library, with its vendored MinHook.
set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/dllmain.cpp
    src/path_utils.cpp
    src/config.cpp
    src/hotkeys.cpp
    src/tracking_runtime.cpp
    src/engine_api.cpp
    src/eyex_block.cpp
    src/screen_calibration.cpp
    src/game_state.cpp
    src/diagnostics.cpp
    src/world_query.cpp
    src/lean_trace.cpp
    src/camera_hook.cpp
    src/hud_crosshair.cpp
    src/fov_reference.cpp
    src/flashlight.cpp
    src/perf_probe.cpp
    # Core builds this only into its optional discovery library.
    cameraunlock-core/cpp/src/memory/rtti_vtable.cpp
    src/window_centering.cpp
)

add_library(DyingLightHeadTracking SHARED ${MOD_SOURCES})

set_target_properties(DyingLightHeadTracking PROPERTIES
    OUTPUT_NAME "DyingLightHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_include_directories(DyingLightHeadTracking PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(DyingLightHeadTracking PRIVATE MOD_VERSION="${PROJECT_VERSION}")
target_link_libraries(DyingLightHeadTracking PRIVATE
    cameraunlock cameraunlock_hooks ws2_32)

if(MSVC)
    target_compile_options(DyingLightHeadTracking PRIVATE /W4 /WX /permissive- /wd4324)
endif()

# Engine-free unit tests: the view basis maths, the aim projection, the
# gameplay gate's reason table and the INI sanitizers. No game, no hooks.
enable_testing()
add_executable(dlht_tests
    tests/test_main.cpp
    tests/view_pose_tests.cpp
    tests/aim_projection_tests.cpp
    tests/gate_rules_tests.cpp
    tests/config_sanitize_tests.cpp
)
target_include_directories(dlht_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(dlht_tests PRIVATE cameraunlock)
if(MSVC)
    target_compile_options(dlht_tests PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME dlht_tests COMMAND dlht_tests)

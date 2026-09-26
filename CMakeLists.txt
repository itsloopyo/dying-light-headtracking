cmake_minimum_required(VERSION 3.20)
project(DyingLightHeadTracking VERSION 0.1.0 LANGUAGES C CXX)

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
    src/legacy_config/legacy_config.cpp
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
# gameplay gate's reason table and the frozen reader's INI sanitizers. No game,
# no hooks.
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

# The config differential test. The oracle is the newest published build's reader and hotkey
# registration with the core sources they compiled, copied verbatim into
# tests/config_differential/oracle. It is two libraries because Hotkeys::Start has to run on
# oracle_fake's recording poller while the reader needs nothing but its own core sources; each
# renames the two namespaces its own way, so neither collides with the other or with the
# current core.
set(DLHT_DIFFERENTIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
set(DLHT_ORACLE_DIR ${DLHT_DIFFERENTIAL_DIR}/oracle)
add_library(dlht_config_oracle STATIC
    ${DLHT_ORACLE_DIR}/src/config.cpp
    ${DLHT_ORACLE_DIR}/core/src/config/ini_reader.cpp
    ${DLHT_ORACLE_DIR}/core/src/logging/file_log.cpp
    ${DLHT_DIFFERENTIAL_DIR}/oracle_config_adapter.cpp
)
target_include_directories(dlht_config_oracle PRIVATE
    ${DLHT_ORACLE_DIR}/src
    ${DLHT_ORACLE_DIR}/core/include
    ${DLHT_DIFFERENTIAL_DIR})
target_compile_definitions(dlht_config_oracle PRIVATE
    cameraunlock=dlht_oracle_core
    DyingLightHeadTracking=dlht_oracle)

add_library(dlht_hotkeys_oracle STATIC
    ${DLHT_ORACLE_DIR}/src/hotkeys.cpp
    ${DLHT_ORACLE_DIR}/core/src/logging/file_log.cpp
    ${DLHT_DIFFERENTIAL_DIR}/oracle_hotkeys_adapter.cpp
)
target_include_directories(dlht_hotkeys_oracle PRIVATE
    ${DLHT_DIFFERENTIAL_DIR}/oracle_fake
    ${DLHT_ORACLE_DIR}/src
    ${DLHT_ORACLE_DIR}/core/include
    ${DLHT_DIFFERENTIAL_DIR})
target_compile_definitions(dlht_hotkeys_oracle PRIVATE
    cameraunlock=dlht_oracle_hk_core
    DyingLightHeadTracking=dlht_oracle_hk)

add_executable(config_differential_tests
    ${DLHT_DIFFERENTIAL_DIR}/differential_tests.cpp
    src/config.cpp
    src/legacy_config/legacy_config.cpp
    src/path_utils.cpp
)
target_include_directories(config_differential_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DLHT_DIFFERENTIAL_DIR})
target_compile_definitions(config_differential_tests PRIVATE
    DLHT_DIFFERENTIAL_DATA="${DLHT_DIFFERENTIAL_DIR}/data"
    DLHT_COMMITTED_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/DyingLightHeadTracking.ini")
target_link_libraries(config_differential_tests PRIVATE
    cameraunlock dlht_config_oracle dlht_hotkeys_oracle)
if(MSVC)
    target_compile_options(config_differential_tests PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME config_differential_tests COMMAND config_differential_tests)

# Core's canonical config lint over the committed file and every file the differential test
# migrated, which it writes beside itself under migrated.
add_test(NAME config_lint COMMAND node
    ${DLHT_DIFFERENTIAL_DIR}/lint-migrated.mjs
    $<TARGET_FILE_DIR:config_differential_tests>/migrated)
set_tests_properties(config_lint PROPERTIES DEPENDS config_differential_tests)

# The committed config against the table, a first start, the toggles' saves and the legacy
# file's ANSI path. `--render-config <path>` makes it write the committed file instead (pixi run
# render-config).
add_executable(config_tests
    tests/config_tests.cpp
    src/config.cpp
    src/legacy_config/legacy_config.cpp
    src/path_utils.cpp
)
target_include_directories(config_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(config_tests PRIVATE
    DLHT_COMMITTED_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/DyingLightHeadTracking.ini")
target_link_libraries(config_tests PRIVATE cameraunlock)
if(MSVC)
    target_compile_options(config_tests PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME config_tests COMMAND config_tests)

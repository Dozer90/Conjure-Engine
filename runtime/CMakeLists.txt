# conjure_runtime - the standalone game runtime (docs/design/06_engine_loop.md 4.2, docs/ARCHITECTURE.md 8,
# docs/design/04_editor.md 8 "Distribution stripping"). One thin main.cpp: parse the engine command line,
# load project.json (the one next to the executable when --project is absent), construct engine::application
# with the project's settings, push game_layer, run; plus the platform backend of runtime_platform.h (UTF-8
# arguments, the executable's directory, the console probe and the message box the diagnostics need).
#
# The executable links conjure::shared and nothing else from the engine (R17), so no editor library and
# no Dear ImGui can reach it except through the umbrella; in Distribution the editor is off, ImGui is not
# fetched and tools/check_distribution_symbols.py proves the binaries clean (tests/runtime/CMakeLists.txt).
#
# The bindings module (docs/design/15_bindings.md, wave 4) is optional at this point: when its target
# exists the runtime adds bindings::scripting_bindings_subsystem before initialise() (startup step 16);
# until then the scripting table keeps the core entries only.

if(NOT TARGET conjure::shared OR NOT TARGET conjure::engine)
    message(STATUS "runtime: conjure::engine is not available, executable skipped")
    return()
endif()

set(CONJURE_RUNTIME_DEFINES "")
if(TARGET conjure::bindings)
    list(APPEND CONJURE_RUNTIME_DEFINES CONJURE_RUNTIME_WITH_BINDINGS=1)
endif()

# The platform backend is selected here rather than with preprocessor guards (as core/system does). On Windows
# the backend needs user32 (MessageBoxW) and shell32 (CommandLineToArgvW), and the manifest makes UTF-8 the
# process code page so the engine's narrow-string paths decode the UTF-8 arguments.
set(CONJURE_RUNTIME_SOURCES
    src/main.cpp
    src/runtime_platform.h
)
set(CONJURE_RUNTIME_PLATFORM_LIBRARIES "")
if(WIN32)
    list(APPEND CONJURE_RUNTIME_SOURCES
        src/runtime_platform_windows.cpp
        src/conjure_runtime.manifest
    )
    list(APPEND CONJURE_RUNTIME_PLATFORM_LIBRARIES user32 shell32)
else()
    list(APPEND CONJURE_RUNTIME_SOURCES src/runtime_platform_posix.cpp)
endif()

# WIN32_GUI: the console window is hidden in Distribution only (conjure_add_executable keeps stdout for
# developers in Debug and Development). main.cpp compensates: without --log-file a Distribution build logs to
# conjure_runtime.log next to the project or the executable, and its own messages also reach the log file and,
# when nothing reads standard error, a message box.
conjure_add_executable(conjure_runtime
    WIN32_GUI
    SOURCES
        ${CONJURE_RUNTIME_SOURCES}
    DEPS
        conjure::shared
        ${CONJURE_RUNTIME_PLATFORM_LIBRARIES}
    DEFINES
        ${CONJURE_RUNTIME_DEFINES}
)

# The exported folder is self-contained: the shared library sits next to the executable
# (export_pipeline step 5), so the run path points at the executable's own directory.
set_target_properties(conjure_runtime PROPERTIES
    OUTPUT_NAME "conjure_runtime"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_RPATH_USE_ORIGIN TRUE
)
if(APPLE)
    set_target_properties(conjure_runtime PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
endif()

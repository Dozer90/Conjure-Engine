# conjure_runtime - the standalone game runtime (docs/design/06_engine_loop.md 4.2, docs/ARCHITECTURE.md 8,
# docs/design/04_editor.md 8 "Distribution stripping"). One thin main.cpp: parse the engine command line,
# load project.json (the one next to the executable when --project is absent), construct engine::application
# with the project's settings, push game_layer, run; plus the platform backend of runtime_platform.h (UTF-8
# arguments, the executable's directory, the console probes and the message boxes the diagnostics and --help
# need).
#
# The executable links conjure::shared and nothing else from the engine (R17), so no editor library and
# no Dear ImGui can reach it except through the umbrella; in Distribution the editor is off, ImGui is not
# fetched and tools/check_distribution_symbols.py proves the binaries clean (tests/runtime/CMakeLists.txt).
#
# The bindings module (docs/design/15_bindings.md) exists whenever scripting is built (CONJURE_WITH_SCRIPTING);
# with its target the runtime adds bindings::scripting_bindings_subsystem before initialise() (startup step 16),
# without it there is no scripting table to fill.

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
# when nothing reads standard error, a message box; --help uses an information box when nothing reads standard
# output.
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

#------------------------------------------------------------------------------
# The third-party notices next to the runtime (06_engine_loop.md D15 and 3.6 step 5, code sweep F200)
#------------------------------------------------------------------------------
# THIRD_PARTY_NOTICES.txt names Conjure's own licence and the licence of everything this tree links into
# conjure_runtime and conjure_shared (cmake/ConjureNotices.cmake writes it at configure time from the
# registrations in third_party/CMakeLists.txt). It is a declared output that conjure_runtime depends on, in every
# configuration, so whichever bin folder an export is pointed at holds it and step 5 ships it with the game.
conjure_ship_third_party_notices(conjure_runtime)

#------------------------------------------------------------------------------
# The Visual C++ runtime next to a Windows Distribution runtime (06_engine_loop.md 3.6 step 5, code sweep F790)
#------------------------------------------------------------------------------
# An MSVC build imports vcruntime140*.dll and msvcp140*.dll (/MD). A Distribution build copies the toolchain's
# redistributable copies of them next to conjure_runtime, where export step 5 finds them and ships them with the
# game: the exported folder then starts on a machine without the Microsoft Visual C++ Redistributable. The
# Universal CRT is part of Windows 10 and later and is not copied; concrt140.dll and vccorlib140.dll, which
# nothing in the engine imports, are not copied either. Debug and Development copy nothing: they are developer
# builds, which run where Visual Studio installed the runtime, and the debug runtime a Debug build imports must
# not be redistributed.
#
# Which folder is shipped is decided in runtime/cmake/ConjureMsvcRuntime.cmake: the one
# InstallRequiredSystemLibraries answers with when its version is not lower than that of the folder the
# compiler's environment names (VCToolsRedistDir), or than the toolset's without one; or else that environment
# folder, taken when its path shows a version. Without either nothing is copied and the configure warns. The
# CTest entry runtime.msvc_runtime_folder_rules holds those rules to fake folders on every platform
# (runtime/cmake/ConjureMsvcRuntimeProbe.cmake).
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ConjureMsvcRuntime.cmake")

# The configurations this tree can build: one for a single-config generator, the list for a multi-config one
# (which offers Distribution only with the editor off, cmake/ConjureConfigs.cmake).
set(CONJURE_RUNTIME_BUILDS_DISTRIBUTION FALSE)
if(CONJURE_IS_MULTI_CONFIG)
    if("Distribution" IN_LIST CMAKE_CONFIGURATION_TYPES)
        set(CONJURE_RUNTIME_BUILDS_DISTRIBUTION TRUE)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Distribution")
    set(CONJURE_RUNTIME_BUILDS_DISTRIBUTION TRUE)
endif()

# The libraries the Distribution configuration copies, readable by tests/runtime (empty: nothing is copied).
set(CONJURE_RUNTIME_MSVC_CRT_LIBRARIES "")
conjure_abi_runtime_facts(runtimeDynamicCrt runtimeAsan runtimeUbsan)
if(MSVC AND runtimeDynamicCrt AND CONJURE_RUNTIME_BUILDS_DISTRIBUTION)
    conjure_msvc_find_crt_libraries(CONJURE_RUNTIME_MSVC_CRT_LIBRARIES)
    if(CONJURE_RUNTIME_MSVC_CRT_LIBRARIES)
        list(GET CONJURE_RUNTIME_MSVC_CRT_LIBRARIES 0 firstCrtLibrary)
        cmake_path(GET firstCrtLibrary PARENT_PATH crtDirectory)
        message(STATUS "runtime: a Distribution build copies the Visual C++ runtime from ${crtDirectory}")
        # One command for both kinds of generator: the copy in Distribution, nothing in any other configuration
        # of a multi-config tree. The condition form of the expression keeps a comma in a path intact.
        set(crtCopyArguments
            copy_if_different ${CONJURE_RUNTIME_MSVC_CRT_LIBRARIES} "$<TARGET_FILE_DIR:conjure_runtime>")
        add_custom_command(TARGET conjure_runtime POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                    "$<$<CONFIG:Distribution>:${crtCopyArguments}>$<$<NOT:$<CONFIG:Distribution>>:true>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    else()
        message(WARNING
            "runtime: no Visual C++ redistributable folder of this compiler's version was found (looked where "
            "InstallRequiredSystemLibraries points and in VCToolsRedistDir). The Distribution build places no "
            "vcruntime140*.dll / msvcp140*.dll next to conjure_runtime, and a game exported from it starts only "
            "where the Microsoft Visual C++ Redistributable is installed.")
    endif()
endif()
set_target_properties(conjure_runtime PROPERTIES
    CONJURE_MSVC_CRT_LIBRARIES "${CONJURE_RUNTIME_MSVC_CRT_LIBRARIES}"
)

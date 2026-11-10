# Managed (C#) projects (docs/design/03_scripting.md 3.10).
#
#   conjure_add_managed_project(NAME <n> PROJECT <csproj> OUTPUT <dir> SOURCES <files...>
#                               [CONFIGURATION <Debug|Release>] [PROPERTIES <Name=Value>...]
#                               [DEPENDS <targets...>] [SKIP_SOURCES_CHECK])
#
#     Creates the target conjure_managed_<n> (part of ALL) that runs
#       dotnet build <csproj> -c <configuration> -o <dir> --artifacts-path <build>/artifacts/<n>
#                    --nologo -clp:NoSummary -p:ContinuousIntegrationBuild=true
#     behind a stamp file that depends on the listed SOURCES, the project file, managed/Directory.Build.props
#     and the stamp files of the managed DEPENDS targets (so a rebuilt SDK rebuilds the projects compiled
#     against it), so an unchanged project costs nothing. No globbing: SOURCES lists every file the csproj compiles, and
#     the CTest entry conjure_managed_sources_match.<n> (label "managed") checks that the csproj's explicit
#     <Compile Include> items and SOURCES name the same files (SKIP_SOURCES_CHECK omits the entry when
#     SOURCES deliberately covers more than the project, such as referenced projects). The configuration
#     defaults to Debug for the Debug build type and Release otherwise. The dotnet program comes from NetHost_DOTNET_EXECUTABLE
#     (cmake/FindNetHost.cmake) or PATH; missing dotnet with CONJURE_BUILD_MANAGED=ON is a configure error.
#
#     -o redirects only the final assemblies; restore and the compile intermediates follow --artifacts-path,
#     one directory per managed target of this build tree, so concurrent build trees of one checkout
#     (AGENTS.md 4) and the variants of one project never share an obj/ and nothing lands in the source
#     tree. The CTest entry conjure_managed_source_tree_clean.<n> (label "managed", once per csproj) fails
#     when an obj/ or bin/ exists beside the project or one of its <ProjectReference>s.
#
#     On a multi-config generator the configuration follows $<CONFIG> and so does the stamp, while the
#     OUTPUT directory is shared: building another configuration rebuilds the assemblies into it, so the
#     last configuration built is the one the executables find (the same rule as the test discovery of
#     the windows-vs presets, CMakePresets.json).
#
#     Result variables: CONJURE_MANAGED_<N>_OUTPUT_DIR (the output directory) and the target properties
#     CONJURE_MANAGED_OUTPUT_DIR and CONJURE_MANAGED_STAMP (the stamp file) on conjure_managed_<n>.
#
# In script mode (cmake -P ConjureManaged.cmake -DCONJURE_MANAGED_CHECK_PROJECT=<csproj>
# -DCONJURE_MANAGED_CHECK_SOURCES=<a;b;c>) the file performs the sources check and exits non-zero on a
# mismatch; with -DCONJURE_MANAGED_CHECK_CLEAN=ON instead it performs the source-tree check.
# conjure_add_managed_project registers both invocations as CTest entries.

if(CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if(NOT CONJURE_MANAGED_CHECK_PROJECT)
        message(FATAL_ERROR "ConjureManaged.cmake -P needs -DCONJURE_MANAGED_CHECK_PROJECT=<csproj>")
    endif()
    file(READ "${CONJURE_MANAGED_CHECK_PROJECT}" projectText)
    get_filename_component(projectDirectory "${CONJURE_MANAGED_CHECK_PROJECT}" DIRECTORY)
    if(CONJURE_MANAGED_CHECK_CLEAN)
        set(projectDirectories "${projectDirectory}")
        string(REGEX MATCHALL "<ProjectReference Include=\"[^\"]+\"" referenceItems "${projectText}")
        foreach(item IN LISTS referenceItems)
            string(REGEX REPLACE "<ProjectReference Include=\"([^\"]+)\"" "\\1" relative "${item}")
            get_filename_component(referenced "${projectDirectory}/${relative}" ABSOLUTE)
            get_filename_component(referencedDirectory "${referenced}" DIRECTORY)
            list(APPEND projectDirectories "${referencedDirectory}")
        endforeach()
        set(polluted "")
        foreach(directory IN LISTS projectDirectories)
            foreach(name IN ITEMS obj bin)
                if(EXISTS "${directory}/${name}")
                    list(APPEND polluted "${directory}/${name}")
                endif()
            endforeach()
        endforeach()
        if(polluted)
            message(FATAL_ERROR
                "managed build outputs in the source tree: ${polluted}\n"
                "dotnet writes beside the project unless it gets an ArtifactsPath; the engine's build trees and tests "
                "pass their own, managed/Directory.Build.props defaults to build/managed_artifacts for builds by hand."
            )
        endif()
        message(STATUS "managed source tree clean (${CONJURE_MANAGED_CHECK_PROJECT})")
        return()
    endif()
    string(REGEX MATCHALL "<Compile Include=\"[^\"]+\"" compileItems "${projectText}")
    set(projectSources "")
    foreach(item IN LISTS compileItems)
        string(REGEX REPLACE "<Compile Include=\"([^\"]+)\"" "\\1" relative "${item}")
        get_filename_component(absolute "${projectDirectory}/${relative}" ABSOLUTE)
        list(APPEND projectSources "${absolute}")
    endforeach()
    set(listedSources "")
    foreach(source IN LISTS CONJURE_MANAGED_CHECK_SOURCES)
        get_filename_component(absolute "${source}" ABSOLUTE)
        list(APPEND listedSources "${absolute}")
    endforeach()
    list(SORT projectSources)
    list(SORT listedSources)
    list(REMOVE_DUPLICATES projectSources)
    list(REMOVE_DUPLICATES listedSources)
    if(NOT projectSources STREQUAL listedSources)
        set(onlyProject ${projectSources})
        list(REMOVE_ITEM onlyProject ${listedSources})
        set(onlyCMake ${listedSources})
        list(REMOVE_ITEM onlyCMake ${projectSources})
        message(FATAL_ERROR
            "managed sources of ${CONJURE_MANAGED_CHECK_PROJECT} differ from the CMake SOURCES list\n"
            "  only in the csproj: ${onlyProject}\n"
            "  only in CMake:      ${onlyCMake}"
        )
    endif()
    message(STATUS "managed sources match (${CONJURE_MANAGED_CHECK_PROJECT})")
    return()
endif()

include_guard(GLOBAL)

set(CONJURE_MANAGED_CMAKE_FILE "${CMAKE_CURRENT_LIST_FILE}")

# dotnet: the host pack finder already looked for it; fall back to PATH.
if(NetHost_DOTNET_EXECUTABLE)
    set(CONJURE_DOTNET_EXECUTABLE "${NetHost_DOTNET_EXECUTABLE}")
else()
    find_program(CONJURE_DOTNET_EXECUTABLE NAMES dotnet)
endif()
if(NOT CONJURE_DOTNET_EXECUTABLE)
    message(FATAL_ERROR
        "CONJURE_BUILD_MANAGED is ON but no dotnet SDK was found; install the .NET 8 SDK, set DOTNET_ROOT, "
        "or configure with -DCONJURE_BUILD_MANAGED=OFF"
    )
endif()

function(conjure_add_managed_project)
    set(options SKIP_SOURCES_CHECK)
    set(oneValueArgs NAME PROJECT OUTPUT CONFIGURATION)
    set(multiValueArgs SOURCES PROPERTIES DEPENDS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_managed_project: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    foreach(required NAME PROJECT OUTPUT SOURCES)
        if(NOT ARG_${required})
            message(FATAL_ERROR "conjure_add_managed_project: ${required} is required")
        endif()
    endforeach()

    get_filename_component(projectPath "${ARG_PROJECT}" ABSOLUTE)
    if(NOT EXISTS "${projectPath}")
        message(FATAL_ERROR "conjure_add_managed_project(${ARG_NAME}): project '${projectPath}' does not exist")
    endif()
    set(sources "")
    foreach(source IN LISTS ARG_SOURCES)
        get_filename_component(absolute "${source}" ABSOLUTE)
        if(NOT EXISTS "${absolute}")
            message(FATAL_ERROR "conjure_add_managed_project(${ARG_NAME}): source '${absolute}' does not exist")
        endif()
        list(APPEND sources "${absolute}")
    endforeach()

    if(NOT ARG_CONFIGURATION)
        set(ARG_CONFIGURATION "$<IF:$<CONFIG:Debug>,Debug,Release>")
    endif()
    set(propertyArguments "")
    foreach(property IN LISTS ARG_PROPERTIES)
        list(APPEND propertyArguments "-p:${property}")
    endforeach()

    set(target "conjure_managed_${ARG_NAME}")
    # One stamp per configuration on multi-config generators: the dotnet configuration follows $<CONFIG>,
    # so a configuration-independent stamp would leave the shared OUTPUT holding the assemblies of
    # whichever configuration was built first.
    if(CMAKE_CONFIGURATION_TYPES)
        set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}.$<CONFIG>.stamp")
    else()
        set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}.stamp")
    endif()
    set(artifactsDirectory "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${ARG_NAME}")
    set(propsFile "${PROJECT_SOURCE_DIR}/managed/Directory.Build.props")

    # A target-level DEPENDS only orders the builds: the file-level dependency on the stamp of each
    # managed dependency is what makes an SDK change rebuild the projects compiled against it. Targets
    # without the property (native ones) contribute nothing here.
    set(dependencyStamps "")
    foreach(dependency IN LISTS ARG_DEPENDS)
        if(TARGET ${dependency})
            get_target_property(dependencyStamp ${dependency} CONJURE_MANAGED_STAMP)
            if(dependencyStamp)
                list(APPEND dependencyStamps "${dependencyStamp}")
            endif()
        endif()
    endforeach()

    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND "${CONJURE_DOTNET_EXECUTABLE}" build "${projectPath}"
                -c "${ARG_CONFIGURATION}" -o "${ARG_OUTPUT}" --artifacts-path "${artifactsDirectory}"
                --nologo -clp:NoSummary -p:ContinuousIntegrationBuild=true ${propertyArguments}
        COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
        DEPENDS ${sources} "${projectPath}" "${propsFile}" ${dependencyStamps} ${ARG_DEPENDS}
        COMMENT "dotnet build ${ARG_NAME} (${ARG_CONFIGURATION}) -> ${ARG_OUTPUT}"
        VERBATIM
    )
    add_custom_target(${target} ALL DEPENDS "${stamp}")
    set_target_properties(${target} PROPERTIES
        FOLDER "Managed"
        CONJURE_MANAGED_OUTPUT_DIR "${ARG_OUTPUT}"
        CONJURE_MANAGED_STAMP "${stamp}"
    )
    if(ARG_DEPENDS)
        add_dependencies(${target} ${ARG_DEPENDS})
    endif()

    string(TOUPPER "${ARG_NAME}" upperName)
    set(CONJURE_MANAGED_${upperName}_OUTPUT_DIR "${ARG_OUTPUT}" PARENT_SCOPE)

    if(CONJURE_BUILD_TESTS AND NOT ARG_SKIP_SOURCES_CHECK)
        string(REPLACE ";" "\\;" escapedSources "${sources}")
        add_test(NAME "conjure_managed_sources_match.${ARG_NAME}"
            COMMAND "${CMAKE_COMMAND}"
                "-DCONJURE_MANAGED_CHECK_PROJECT=${projectPath}"
                "-DCONJURE_MANAGED_CHECK_SOURCES=${escapedSources}"
                -P "${CONJURE_MANAGED_CMAKE_FILE}"
        )
        set_tests_properties("conjure_managed_sources_match.${ARG_NAME}" PROPERTIES LABELS "managed" TIMEOUT 60)
    endif()
    if(CONJURE_BUILD_TESTS)
        # One entry per project file, named after the first target that builds it (the variants of one
        # project would otherwise register the same check three times).
        string(MAKE_C_IDENTIFIER "${projectPath}" projectKey)
        get_property(cleanCheckRegistered GLOBAL PROPERTY "CONJURE_MANAGED_CLEAN_CHECK_${projectKey}")
        if(NOT cleanCheckRegistered)
            set_property(GLOBAL PROPERTY "CONJURE_MANAGED_CLEAN_CHECK_${projectKey}" ON)
            add_test(NAME "conjure_managed_source_tree_clean.${ARG_NAME}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DCONJURE_MANAGED_CHECK_PROJECT=${projectPath}"
                    -DCONJURE_MANAGED_CHECK_CLEAN=ON
                    -P "${CONJURE_MANAGED_CMAKE_FILE}"
            )
            set_tests_properties("conjure_managed_source_tree_clean.${ARG_NAME}" PROPERTIES LABELS "managed" TIMEOUT 60)
        endif()
    endif()
endfunction()

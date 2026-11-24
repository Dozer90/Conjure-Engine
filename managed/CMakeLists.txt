# Managed (C#) assemblies (docs/design/03_scripting.md 3.10, 4.1): the SDK anchored by Conjure.Bootstrap,
# the three hot-reload variants of Conjure.TestScripts and the game template. Every project lists its
# compile items explicitly; conjure_add_managed_project checks that the lists agree.

include(ConjureManaged)

# This directory is added before the root enables testing; its CTest entries need the flag themselves.
if(CONJURE_BUILD_TESTS)
    enable_testing()
endif()

# Next to the executables, where script_host_config::managedDirectory points by default.
set(CONJURE_MANAGED_OUTPUT_ROOT "${PROJECT_BINARY_DIR}/bin/managed")

set(CONJURE_MANAGED_SDK_SOURCES
    Conjure.Managed/Generated/NativeApi.g.cs
    Conjure.Managed/Interop/FrameBlock.cs
    Conjure.Managed/Interop/ManagedApi.cs
    Conjure.Managed/Interop/NativeBridge.cs
    Conjure.Managed/Interop/Records.cs
    Conjure.Managed/Interop/ScriptStatus.cs
    Conjure.Managed/Runtime/AssemblyLoader.cs
    Conjure.Managed/Runtime/FieldBlob.cs
    Conjure.Managed/Runtime/GameAssemblyLoadContext.cs
    Conjure.Managed/Runtime/Guard.cs
    Conjure.Managed/Runtime/InstanceTable.cs
    Conjure.Managed/Runtime/JsonText.cs
    Conjure.Managed/Runtime/LayoutHash.cs
    Conjure.Managed/Runtime/NameHash.cs
    Conjure.Managed/Runtime/PhaseList.cs
    Conjure.Managed/Runtime/ScriptCatalog.cs
    Conjure.Managed/Runtime/ScriptClassInfo.cs
    Conjure.Managed/Runtime/ScriptRuntime.cs
    Conjure.Managed/Api/AssetRef.cs
    Conjure.Managed/Api/Attributes.cs
    Conjure.Managed/Api/Audio.cs
    Conjure.Managed/Api/Collision.cs
    Conjure.Managed/Api/Color.cs
    Conjure.Managed/Api/Component.cs
    Conjure.Managed/Api/Entity.cs
    Conjure.Managed/Api/GamepadButton.cs
    Conjure.Managed/Api/Input.cs
    Conjure.Managed/Api/Key.cs
    Conjure.Managed/Api/Log.cs
    Conjure.Managed/Api/MouseButton.cs
    Conjure.Managed/Api/Physics.cs
    Conjure.Managed/Api/Prefab.cs
    Conjure.Managed/Api/Scene.cs
    Conjure.Managed/Api/ScriptBehaviour.cs
    Conjure.Managed/Api/Time.cs
    Conjure.Managed/Api/Transform.cs
    Conjure.Managed/Api/Components/LocalTransform.cs
    Conjure.Managed/Api/Dod/ChunkView.cs
    Conjure.Managed/Api/Dod/ComponentQuery.cs
    Conjure.Managed/Api/Dod/ComponentType.cs
    Conjure.Managed/Api/Dod/QueryExclude.cs
    Conjure.Managed/Interop/AssetRefBinder.cs
    Conjure.Managed/Interop/BindingStatus.g.cs
    Conjure.Managed/Interop/ConjureNativeException.cs
    Conjure.Managed/Interop/NativeCheck.cs
    Conjure.Managed/Interop/Utf8Marshal.cs
    Conjure.Managed/Api/AssetId.cs
    Conjure.Managed/Api/AssetState.cs
    Conjure.Managed/Api/AssetTypes.cs
    Conjure.Managed/Api/AudioSource.cs
    Conjure.Managed/Api/CharacterController.cs
    Conjure.Managed/Api/Diagnostics.cs
    Conjure.Managed/Api/Entity.Hierarchy.cs
    Conjure.Managed/Api/Input.Bindings.cs
    Conjure.Managed/Api/LayerMask.cs
    Conjure.Managed/Api/Modules.cs
    Conjure.Managed/Api/RaycastHit.cs
    Conjure.Managed/Api/Rigidbody.cs
    Conjure.Managed/Api/Components/EntityHandle.cs
    Conjure.Managed/Api/Components/NativeBool.cs
    Conjure.Managed/Api/Components/StringId.cs
    Conjure.Managed/Api/Components/Audio/AudioMirrors.cs
    Conjure.Managed/Api/Components/Physics/PhysicsMirrors.cs
    Conjure.Managed/Api/Components/Scene/SceneMirrors.cs
    Conjure.Managed/Api/Components/Renderer/RendererMirrors.cs
    Conjure.Managed/Api/Camera.cs
    Conjure.Managed/Api/Entity.Spawn.cs
    Conjure.Managed/Api/Ray.cs
    Conjure.Managed/Api/Screen.cs
)

# The SDK: building the bootstrap builds Conjure.Managed through its project reference and lays both
# out, with the runtime config and Conjure.Managed.xml, in one directory (the managed_directory of
# script_host_config).
conjure_add_managed_project(
    NAME sdk
    PROJECT Conjure.Bootstrap/Conjure.Bootstrap.csproj
    OUTPUT "${CONJURE_MANAGED_OUTPUT_ROOT}"
    SOURCES
        Conjure.Bootstrap/Program.cs
        Conjure.Bootstrap/runtimeconfig.template.json
        ${CONJURE_MANAGED_SDK_SOURCES}
    SKIP_SOURCES_CHECK
)
# The bootstrap's SOURCES cover the referenced SDK too, so its project is checked on its own here (the
# check runs in script mode without a current source directory, hence the absolute list).
if(CONJURE_BUILD_TESTS)
set(CONJURE_MANAGED_SDK_SOURCES_ABSOLUTE ${CONJURE_MANAGED_SDK_SOURCES})
list(TRANSFORM CONJURE_MANAGED_SDK_SOURCES_ABSOLUTE PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
string(REPLACE ";" "\\;" CONJURE_MANAGED_SDK_SOURCES_ESCAPED "${CONJURE_MANAGED_SDK_SOURCES_ABSOLUTE}")
add_test(NAME conjure_managed_sources_match.managed
    COMMAND "${CMAKE_COMMAND}"
        "-DCONJURE_MANAGED_CHECK_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Managed/Conjure.Managed.csproj"
        "-DCONJURE_MANAGED_CHECK_SOURCES=${CONJURE_MANAGED_SDK_SOURCES_ESCAPED}"
        -P "${CONJURE_MANAGED_CMAKE_FILE}"
)
set_tests_properties(conjure_managed_sources_match.managed PROPERTIES LABELS "managed" TIMEOUT 60)
add_test(NAME conjure_managed_sources_match.bootstrap
    COMMAND "${CMAKE_COMMAND}"
        "-DCONJURE_MANAGED_CHECK_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Bootstrap/Conjure.Bootstrap.csproj"
        "-DCONJURE_MANAGED_CHECK_SOURCES=${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Bootstrap/Program.cs"
        -P "${CONJURE_MANAGED_CMAKE_FILE}"
)
set_tests_properties(conjure_managed_sources_match.bootstrap PROPERTIES LABELS "managed" TIMEOUT 60)
# The SDK's XML documentation ships beside Conjure.Managed.dll (game projects reference both); hashing a
# missing file fails the entry.
add_test(NAME conjure_managed_xml_docs.sdk
    COMMAND "${CMAKE_COMMAND}" -E sha256sum "${CONJURE_MANAGED_OUTPUT_ROOT}/Conjure.Managed.xml"
)
set_tests_properties(conjure_managed_xml_docs.sdk PROPERTIES LABELS "managed" TIMEOUT 60)
endif()

if(TARGET conjure_scripting)
    add_dependencies(conjure_scripting conjure_managed_sdk)
endif()

# The export-flavoured SDK (owner decision OWN-INSTALL): Conjure.Bootstrap and Conjure.Managed published as an export
# publishes them (engine/src/engine/export_commands.cpp: Release, framework-dependent, no apphost, a CI build that
# imports export_source_roots.targets so no folder of this machine stays in the assemblies or their .pdb files) into
# bin/managed_export. An installed editor carries this folder as the managed/ folder beside it: it runs its scripts
# from it and project.export copies it instead of publishing the bootstrap from a source tree, which it does not
# have. The assemblies must match the native table of the editor that ships them, so the folder belongs to the same
# build; a packaging step copies it. It is rebuilt from scratch, and conjure_managed_sdk.json (the manifest
# export_pipeline::MANAGED_SDK_MANIFEST_NAME, naming CONJURE_NATIVE_API_HASH) is written last, so a failed publish
# leaves no folder an export would take for a complete SDK. The .pdb files stay: the export copies them only when
# the project keeps its debug symbols (scripting.debug_symbols in project.json, scripting_settings::exportDebugSymbols).
set(CONJURE_MANAGED_SDK_EXPORT_OUTPUT "${PROJECT_BINARY_DIR}/bin/managed_export")
set(managedNativeApiHashHeader "${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/native_api_hash.h")
set(managedSourceRootsTargets "${PROJECT_SOURCE_DIR}/engine/src/engine/export_source_roots.targets")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${managedNativeApiHashHeader}")
file(STRINGS "${managedNativeApiHashHeader}" managedNativeApiHashLine REGEX "^#define CONJURE_NATIVE_API_HASH ")
if(NOT managedNativeApiHashLine MATCHES "^#define CONJURE_NATIVE_API_HASH (0x[0-9A-Fa-f]+)ull$")
    message(FATAL_ERROR "managed: ${managedNativeApiHashHeader} defines no CONJURE_NATIVE_API_HASH 0x...ull")
endif()
set(managedNativeApiHash "${CMAKE_MATCH_1}")
set(managedSdkManifest "${CMAKE_CURRENT_BINARY_DIR}/sdk_export/conjure_managed_sdk.json")
# Written only when the text changes; the stamp below depends on it, so a new hash publishes again.
set(managedSdkManifestText "{\n")
string(APPEND managedSdkManifestText
    "  \"schema_version\": 1,\n"
    "  \"kind\": \"conjure_managed_sdk_export\",\n"
    "  \"host_mode\": \"framework_dependent\",\n"
    "  \"native_api_hash\": \"@managedNativeApiHash@\"\n"
    "}\n"
)
file(CONFIGURE OUTPUT "${managedSdkManifest}" CONTENT "${managedSdkManifestText}" @ONLY)
set(managedSdkExportStamp "${CMAKE_CURRENT_BINARY_DIR}/conjure_managed_sdk_export.stamp")
set(managedSdkExportProjects
    "${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Bootstrap/Conjure.Bootstrap.csproj"
    "${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Managed/Conjure.Managed.csproj"
)
set(managedSdkExportSources ${CONJURE_MANAGED_SDK_SOURCES} Conjure.Bootstrap/Program.cs
    Conjure.Bootstrap/runtimeconfig.template.json)
list(TRANSFORM managedSdkExportSources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
add_custom_command(
    OUTPUT "${managedSdkExportStamp}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}"
    COMMAND "${CONJURE_DOTNET_EXECUTABLE}" publish
            "${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Bootstrap/Conjure.Bootstrap.csproj"
            -c Release -o "${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}" --self-contained false
            --artifacts-path "${CMAKE_CURRENT_BINARY_DIR}/artifacts/sdk_export"
            -p:ArtifactsPivots=release_export_framework_dependent
            -p:ContinuousIntegrationBuild=true
            "-p:CustomAfterMicrosoftCommonTargets=${managedSourceRootsTargets}"
            -p:UseAppHost=false
            --nologo -clp:NoSummary
    COMMAND "${CMAKE_COMMAND}" -E copy "${managedSdkManifest}" "${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}/"
    COMMAND "${CMAKE_COMMAND}" -E touch "${managedSdkExportStamp}"
    DEPENDS
        ${managedSdkExportSources}
        ${managedSdkExportProjects}
        "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
        ${CONJURE_DOTNET_GLOBAL_JSON}
        "${managedSourceRootsTargets}"
        "${managedSdkManifest}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "dotnet publish sdk_export (Release, export properties) -> ${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}"
    VERBATIM
)
add_custom_target(conjure_managed_sdk_export ALL DEPENDS "${managedSdkExportStamp}")
set_target_properties(conjure_managed_sdk_export PROPERTIES
    FOLDER "Managed"
    CONJURE_MANAGED_OUTPUT_DIR "${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}"
    CONJURE_MANAGED_STAMP "${managedSdkExportStamp}"
)
# Only an order: the two publish into separate artifacts directories, but one dotnet at a time restores the SDK's
# packages.
add_dependencies(conjure_managed_sdk_export conjure_managed_sdk)
set(CONJURE_MANAGED_SDK_EXPORT_DIR "${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}" CACHE INTERNAL
    "directory holding the export-flavoured Conjure.Managed.dll and bootstrap with their manifest")
# The published assemblies and .pdb files name neither the checkout nor this build tree
# (tools/check_distribution_paths.py --binary looks for both). Without Python there is no entry.
if(CONJURE_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(managedSdkExportBinaries "")
        foreach(assembly IN ITEMS Conjure.Bootstrap Conjure.Managed)
            foreach(extension IN ITEMS dll pdb)
                list(APPEND managedSdkExportBinaries
                    --binary "${CONJURE_MANAGED_SDK_EXPORT_OUTPUT}/${assembly}.${extension}")
            endforeach()
        endforeach()
        add_test(NAME conjure_managed_sdk_export_paths.sdk_export
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/check_distribution_paths.py"
                ${managedSdkExportBinaries} --root "${PROJECT_BINARY_DIR}"
        )
        set_tests_properties(conjure_managed_sdk_export_paths.sdk_export PROPERTIES LABELS "managed" TIMEOUT 120)
    else()
        message(STATUS "managed: no Python interpreter, conjure_managed_sdk_export_paths.sdk_export not registered")
    endif()
endif()

# The test scripts: one project, three variants, built one after another (they share the SDK project).
set(CONJURE_TEST_SCRIPT_SOURCES
    Conjure.TestScripts/BindingProbes.cs
    Conjure.TestScripts/CameraProbes.cs
    Conjure.TestScripts/Collider.cs
    Conjure.TestScripts/DestroyProbe.cs
    Conjure.TestScripts/DodScripts.cs
    Conjure.TestScripts/DrawProbe.cs
    Conjure.TestScripts/FieldKinds.cs
    Conjure.TestScripts/FrameStatsProbe.cs
    Conjure.TestScripts/InspectorProbe.cs
    Conjure.TestScripts/LanguageVersionProbe.cs
    Conjure.TestScripts/LeakyTimer.cs
    Conjure.TestScripts/LifecycleProbe.cs
    Conjure.TestScripts/Mover.cs
    Conjure.TestScripts/NativeCallProbe.cs
    Conjure.TestScripts/Probes.cs
    Conjure.TestScripts/QueryFlagsProbe.cs
    Conjure.TestScripts/RegressionProbes.cs
    Conjure.TestScripts/ReloadProbe.cs
    Conjure.TestScripts/SpawnProbes.cs
    Conjure.TestScripts/Spawner.cs
    Conjure.TestScripts/Throwers.cs
)

if(CONJURE_BUILD_TESTS)
    set(previousVariant conjure_managed_sdk)
    set(skipCheck "")
    foreach(variant V1 V2 V3)
        string(TOLOWER "${variant}" lowerVariant)
        conjure_add_managed_project(
            NAME test_scripts_${lowerVariant}
            PROJECT Conjure.TestScripts/Conjure.TestScripts.csproj
            OUTPUT "${CONJURE_MANAGED_OUTPUT_ROOT}/test_scripts/${lowerVariant}"
            PROPERTIES "ScriptVariant=${variant}"
            SOURCES ${CONJURE_TEST_SCRIPT_SOURCES}
            DEPENDS ${previousVariant}
            ${skipCheck}
        )
        set(previousVariant conjure_managed_test_scripts_${lowerVariant})
        set(skipCheck SKIP_SOURCES_CHECK)     # one project, one check
    endforeach()
endif()

# The game template is checked but not built: it needs a project to point ConjureManagedDir at.
if(CONJURE_BUILD_TESTS)
    add_test(NAME conjure_managed_sources_match.template
        COMMAND "${CMAKE_COMMAND}"
            "-DCONJURE_MANAGED_CHECK_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Templates/Game/Game.csproj"
            "-DCONJURE_MANAGED_CHECK_SOURCES=${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Templates/Game/Player.cs"
            -P "${CONJURE_MANAGED_CMAKE_FILE}"
    )
    set_tests_properties(conjure_managed_sources_match.template PROPERTIES LABELS "managed" TIMEOUT 60)
endif()

set(CONJURE_MANAGED_SDK_DIR "${CONJURE_MANAGED_OUTPUT_ROOT}" CACHE INTERNAL "directory holding Conjure.Managed.dll and the bootstrap")
set(CONJURE_MANAGED_TEST_SCRIPTS_DIR "${CONJURE_MANAGED_OUTPUT_ROOT}/test_scripts" CACHE INTERNAL "v1/v2/v3 builds of Conjure.TestScripts")
set(CONJURE_MANAGED_TEST_SCRIPTS_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/Conjure.TestScripts/Conjure.TestScripts.csproj" CACHE INTERNAL "")
set(CONJURE_MANAGED_BOOTSTRAP_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Bootstrap/Conjure.Bootstrap.csproj" CACHE INTERNAL "")

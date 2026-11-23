---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# project.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/project.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p><code><a href="project/project.html">project</a>.json</code>: schema version 1, <code>snake_case</code> keys. Unknown top-level keys log one <code>WARN</code> each and are preserved in <code>extensions</code> on save, unless <code>extensions</code> already has a member of that name, which wins; relative paths resolve against the <code><a href="project/project.html">project</a></code> root; &quot;<code>fixed_hz</code>&quot; is stored for readability and converted to fixedDeltaSeconds. The export pipeline writes the same file with &quot;cooked&quot;: <code>true</code>.</p>

## Structs

| File | Description |
|---|---|
| [`project_description`](project/project-description.md) |  |
| [`project_module_entry`](project/project-module-entry.md) | <code>A</code> module manifest the <code><a href="project/project.html">project</a></code> refers to: the root <code>module.json</code> (the single game module, required) or one found under <code>modules.search_paths</code>. |

## Classes

| File | Description |
|---|---|
| [`project`](project/project.md) |  |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-PROJECT_FILE_NAME"></a><code>PROJECT_FILE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"project.json"</span></code> |  |
| <a id="variable-PROJECT_SCHEMA_VERSION"></a><code>PROJECT_SCHEMA_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="project"></a>
<a id="project_description"></a>
<a id="project_descriptionassets"></a>
<a id="project_descriptionassetsroot"></a>
<a id="project_descriptionaudio"></a>
<a id="project_descriptioncooked"></a>
<a id="project_descriptioncookedroot"></a>
<a id="project_descriptionengineversion"></a>
<a id="project_descriptionextensions"></a>
<a id="project_descriptionlayers"></a>
<a id="project_descriptionmodules"></a>
<a id="project_descriptionname"></a>
<a id="project_descriptionphysics"></a>
<a id="project_descriptionphysicssection"></a>
<a id="project_descriptionrendering"></a>
<a id="project_descriptionschemaversion"></a>
<a id="project_descriptionscripting"></a>
<a id="project_descriptionstartupscene"></a>
<a id="project_descriptiontime"></a>
<a id="project_descriptionversion"></a>
<a id="project_descriptionwindow"></a>
<a id="project_file_name"></a>
<a id="project_module_entry"></a>
<a id="project_module_entrymanifestpath"></a>
<a id="project_module_entryname"></a>
<a id="project_module_entryoperator"></a>
<a id="project_module_entryrequired"></a>
<a id="project_schema_version"></a>
<a id="projectapply_to"></a>
<a id="projectassets_directory"></a>
<a id="projectcooked_directory"></a>
<a id="projectdescription"></a>
<a id="projectfile_path"></a>
<a id="projectload"></a>
<a id="projectmdescription"></a>
<a id="projectmodule_manifests"></a>
<a id="projectmrootdirectory"></a>
<a id="projectoperator"></a>
<a id="projectparse"></a>
<a id="projectproject"></a>
<a id="projectproject_1"></a>
<a id="projectroot_directory"></a>
<a id="projectsave"></a>
<a id="projectto_json"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

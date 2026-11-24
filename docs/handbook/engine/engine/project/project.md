---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [project.h](../project.md)

# project
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/project.h` |
| Source File Path | `engine/src/engine/project.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
class CONJURE_API project
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">project();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">project(
    <span class="k">const</span> <a href="project.html">project</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">project(
    <a href="project.html">project</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~project();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDescription</code></td><td><code class="signature"><a href="project-description.html">project_description</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Root<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `apply_to` {#function-apply-to}

<div class="function-declaration" id="function-apply-to-1"><pre><code><span class="k">void</span> apply_to(
    <a href="../engine-config/engine-config.html">engine_config</a> &amp;config) <span class="k">const</span>;</code></pre></div>

<p>Copies the settings sections, the module settings and <code>startupScene</code> (as startupSceneOverride when the config has none) into a config; headless, editor, maxFrames, jobs, memory, pacing, log settings and projectPath are untouched. <code>rendering.enabled</code>, the rendering and audio backends, <code>assets.hotReload</code>, <code>scripting.enabled</code> and the window size are copied only when the config is not headless, so <code>headless_defaults()</code> survive a <code><a href="project.html">project</a></code> (the command line turns scripting back on with --scripting); <code>audio.enabled</code> is copied either way. <code>rendering.gpuValidation</code> is only raised, to the higher of the two levels: a saved <code><a href="project.html">project</a></code> always names one, usually <code>OFF</code>, and that must not drop the debug layer of a Debug build (--gpu-validation sets any level).</p>

#### `assets_directory` {#function-assets-directory}

<div class="function-declaration" id="function-assets-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path assets_directory() <span class="k">const</span>;</code></pre></div>

<p><code>assetsRoot</code> resolved against the root (an absolute <code>assetsRoot</code> is returned as is).</p>

#### `cooked_directory` {#function-cooked-directory}

<div class="function-declaration" id="function-cooked-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path cooked_directory() <span class="k">const</span>;</code></pre></div>

<p><code>cookedRoot</code> resolved against the root.</p>

#### `description` {#function-description}

<div class="function-declaration" id="function-description-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="project-description.html">project_description</a> &amp;description() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-description-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="project-description.html">project_description</a> &amp;description() <span class="k">noexcept</span>;</code></pre></div>

#### `file_path` {#function-file-path}

<div class="function-declaration" id="function-file-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path file_path() <span class="k">const</span>;</code></pre></div>

<p>&lt;root&gt;/<code><a href="project.html">project</a>.json</code>.</p>

#### `load` {#function-load}

<div class="function-declaration" id="function-load-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="project.html">project</a>&gt; load(
    <span class="k">const</span> std::filesystem::path &amp;directory);</code></pre></div>

<p>Reads &lt;directory&gt;/<code><a href="project.html">project</a>.json</code>. <code>PROJECT_NOT_FOUND</code> when the directory or file is missing, then as <code><a href="#function-parse-1">parse</a>()</code>.</p>

#### `module_manifests` {#function-module-manifests}

<div class="function-declaration" id="function-module-manifests-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="project-module-entry.html">project_module_entry</a>&gt; module_manifests() <span class="k">const</span>;</code></pre></div>

<p>The module manifests this <code><a href="project.html">project</a></code> names: &lt;root&gt;/<code>module.json</code> when it exists (required, the game module) and every &lt;searchPath&gt;/*/<code>module.json</code> found on disk, in path order; <code>required</code> follows <code>modules.required</code> by directory name.</p>

#### `parse` {#function-parse}

<div class="function-declaration" id="function-parse-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="project.html">project</a>&gt; parse(
    std::string_view text,
    std::filesystem::path rootDirectory);</code></pre></div>

<p>Parses <code><a href="project.html">project</a>.json</code> text for a <code><a href="project.html">project</a></code> rooted at <code>rootDirectory</code>. <code>PROJECT_PARSE_FAILED</code> (with yyjson&#x27;s line and column), <code>PROJECT_SCHEMA_TOO_NEW</code>, <code>PROJECT_INVALID</code> (a non-object root, a missing or empty &quot;name&quot;, a member of the wrong type).</p>

#### `root_directory` {#function-root-directory}

<div class="function-declaration" id="function-root-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;root_directory() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `save` {#function-save}

<div class="function-declaration" id="function-save-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; save() <span class="k">const</span>;</code></pre></div>

<p>Writes &lt;root&gt;/<code><a href="project.html">project</a>.json</code> (pretty printed) from <code><a href="#function-to-json-1">to_json</a>()</code>. <code>IO_FAILURE</code> from the file system.</p>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> to_json() <span class="k">const</span>;</code></pre></div>

<p>The document <code><a href="#function-save-1">save</a>()</code> writes: every section in schema order, &quot;<code>fixed_hz</code>&quot; instead of the delta, unknown keys inside &quot;extensions&quot;. &quot;physics&quot; and &quot;rendering&quot; are the raw sections the file had (<code>physicsSection</code>, <code>renderingSection</code>) with the engine&#x27;s own keys written over them, so keys the engine does not read, <code>rendering.defaults</code> among them, are kept as they were.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="project.html">project</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="project.html">project</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="project.html">project</a> &amp;<span class="k">operator</span>=(
    <a href="project.html">project</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

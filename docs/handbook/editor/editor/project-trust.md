---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# project_trust.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/project_trust.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Project trust. Opening a project runs code from its folder with the rights of the user who runs the editor: the engine loads its native modules and its game assembly while it starts, and <code>script.compile</code>, <code>module.build</code> and <code>play.start</code> run more of it afterwards. The editor therefore opens a project only once somebody answered for it, and <code><a href="project-trust/project-trust.html">project_trust</a></code> is where the answers are kept:</p>

## Structs

| File | Description |
|---|---|
| [`trusted_project`](project-trust/trusted-project.md) | One remembered answer, as the trust store holds it. |

## Classes

| File | Description |
|---|---|
| [`project_trust`](project-trust/project-trust.md) |  |

## Enumerations

### `trust_state` {#symbol-trust-state}

```cpp
enum class trust_state : u8 {
    UNTRUSTED = 0,          // nobody answered for the project: the editor does not open it
    SESSION,                // trusted until this editor exits
    REMEMBERED              // trusted in the trust store of this user on this machine
};
```

What is known about a project. Ordered: a greater value is the longer-lived answer.

## Functions

### Public

#### `default_trust_store_file` {#function-default-trust-store-file}

<div class="function-declaration" id="function-default-trust-store-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/result/result.html">core::result</a>&lt;std::filesystem::path&gt; default_trust_store_file();</code></pre></div>

<p>The trust store <code>conjure_editor</code> uses when --trust-store names none: <code>STORE_FILE_NAME</code> in the folder the operating system gives a program for the state it keeps per user on one machine. Windows: %LOCALAPPDATA%/Conjure (the known folder, which does not roam to other machines). Linux: $<code>XDG_STATE_HOME</code>/conjure when that variable holds an absolute path, otherwise $<code>HOME</code>/.local/state/conjure. macOS: $<code>HOME</code>/Library/Application Support/Conjure. <code>NOT_FOUND</code> when the system names no such folder (no known folder; neither variable holds an absolute path): nothing is remembered then. The folder is not created by this call. Only the executable calls it: an <code><a href="editor-application/editor-application.html">editor_application</a></code> given no store file remembers nothing, so tests and embedders never touch the user&#x27;s folder by accident.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="project-trust.html#symbol-trust-state">trust_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire spelling of a state: &quot;untrusted&quot;, &quot;session&quot; or &quot;remembered&quot;; &quot;&quot; for a value outside the enumeration.</p>

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
<a id="default_trust_store_file"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="project_trust"></a>
<a id="project_trustis_trusted"></a>
<a id="project_trustmax_origin_bytes"></a>
<a id="project_trustmax_remembered_projects"></a>
<a id="project_trustmax_store_bytes"></a>
<a id="project_trustmsession"></a>
<a id="project_trustmstorefile"></a>
<a id="project_trustoperator"></a>
<a id="project_trustproject_key"></a>
<a id="project_trustproject_trust"></a>
<a id="project_trustproject_trust_1"></a>
<a id="project_trustread_store"></a>
<a id="project_trustremembered"></a>
<a id="project_trustrevoke"></a>
<a id="project_trustset_store_file"></a>
<a id="project_truststate_of"></a>
<a id="project_truststate_of_key"></a>
<a id="project_truststore_file"></a>
<a id="project_truststore_file_name"></a>
<a id="project_truststore_schema_version"></a>
<a id="project_trusttrust"></a>
<a id="project_trusttrust_for_session"></a>
<a id="project_trustwrite_store"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="trust_state"></a>
<a id="trust_stateremembered"></a>
<a id="trust_statesession"></a>
<a id="trust_stateuntrusted"></a>
<a id="trusted_project"></a>
<a id="trusted_projectoperator"></a>
<a id="trusted_projectorigin"></a>
<a id="trusted_projectpath"></a>
<a id="trusted_projecttrustedat"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

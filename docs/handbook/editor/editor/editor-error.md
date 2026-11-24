---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# editor_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_error.h` |
| API ↗ | [The AI control protocol](../../../api/editor/ai-protocol.md), [The editor](../../../api/editor/index.md) |

<p>The editor error category. The protocol&#x27;s stable error strings and the editor&#x27;s own <code><a href="../../engine/core/error/error.html">core::error</a></code> codes are one enumeration: a handler returns <code>core::make_error(code, message)</code> and the dispatcher writes <code>to_string(code)</code> into the response. Errors of other modules (ecs, scene, assets, core) that reach a response are mapped through <code>classify_error()</code>.</p>

## Enumerations

### `protocol_error_code` {#symbol-protocol-error-code}

```cpp
enum class protocol_error_code : u16 {
    NONE = 0,
    PARSE_ERROR,            // the line is not JSON
    INVALID_REQUEST,        // the JSON is not a request object (missing cmd, non-object args)
    MESSAGE_TOO_LARGE,      // a line above MAX_MESSAGE_BYTES; the client is disconnected afterwards
    UNKNOWN_COMMAND,        // no descriptor with that name; details.didYouMean lists the closest names
    INVALID_ARGS,           // schema violation or a semantic argument error; the message names the path
    ENTITY_NOT_FOUND,       // an entity reference resolved to no live entity
    COMPONENT_NOT_FOUND,    // the entity does not carry the component
    TYPE_NOT_FOUND,         // no component, asset or script class type with that name
    PATH_NOT_FOUND,         // a reflection path does not resolve, or a file or folder does not exist
    ASSET_NOT_FOUND,        // an asset reference (uuid or asset path) names no asset in the database
    PROJECT_NOT_OPEN,       // the command needs a project
    SCENE_NOT_OPEN,         // the command needs a scene
    WRONG_PLAY_STATE,       // the command is refused in the current play state (details.state)
    NOT_AVAILABLE,          // the feature is compiled out, the backend is null or a tool is missing
    IO_ERROR,               // a file or folder that exists could not be read, or a write failed
    COMPILE_FAILED,         // the managed build or a module build reported errors
    BUSY,                   // too many pending asynchronous requests for the client
    TIMEOUT,                // an asynchronous command did not complete in time
    CANCELLED,              // the client disconnected before an asynchronous reply
    UNAUTHORISED,           // where a token is required, a first request that is not a valid session.hello
                            // (the client is disconnected); a later hello with a wrong token; a command the
                            // client's token does not open (details {required, granted}; the client stays)
    PROJECT_NOT_TRUSTED,    // nobody answered for the project on this machine; project.trust comes first
    STALE_STATE,            // an asynchronous command found the scene, the play state or its target changed
                            // between dispatch and completion; it changed nothing
    INTERNAL,               // an invariant failed; the message says which
    COUNT
};
```

Every error a protocol response may carry. The names travel on the wire as strings; the numeric values never do.

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY( conjure::editor::protocol_error_code, "editor", conjure::editor::editor_error_name )
```

## Functions

### Public

#### `all_error_codes` {#function-all-error-codes}

<div class="function-declaration" id="function-all-error-codes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">const</span> <a href="editor-error.html#symbol-protocol-error-code">protocol_error_code</a>&gt; all_error_codes() <span class="k">noexcept</span>;</code></pre></div>

<p>Every code except <code>NONE</code> and <code>COUNT</code>, in enumeration order (<code>editor.schema</code> lists them).</p>

#### `classify_error` {#function-classify-error}

<div class="function-declaration" id="function-classify-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="editor-error.html#symbol-protocol-error-code">protocol_error_code</a> classify_error(
    <span class="k">const</span> <a href="../../engine/core/error/error.html">core::error</a> &amp;failure) <span class="k">noexcept</span>;</code></pre></div>

<p>The protocol code of any <code><a href="../../engine/core/error/error.html">core::error</a></code>: editor errors map to their own code; scene <code>GUID_NOT_FOUND</code> and ecs <code>ENTITY_NOT_ALIVE</code> to <code>ENTITY_NOT_FOUND</code>; ecs <code>COMPONENT_NOT_PRESENT</code> to <code>COMPONENT_NOT_FOUND</code>; <code>UNKNOWN_COMPONENT</code> to <code>TYPE_NOT_FOUND</code>; assets <code>NOT_FOUND</code> to <code>ASSET_NOT_FOUND</code>; core <code>IO_FAILURE</code> and assets <code>IO_ERROR</code> to <code>IO_ERROR</code>; core <code>INVALID_ARGUMENT</code>, <code>OUT_OF_RANGE</code>, <code>PARSE_FAILURE</code> and ecs <code>SERIALIZATION_FAILED</code> to <code>INVALID_ARGS</code>; core <code>NOT_FOUND</code> and engine <code>PROJECT_NOT_FOUND</code> (a missing file or folder) to <code>PATH_NOT_FOUND</code>; core <code>UNSUPPORTED</code> and <code>NOT_IMPLEMENTED</code> to <code>NOT_AVAILABLE</code>; core <code>TIMEOUT</code> to <code>TIMEOUT</code>; everything else to <code>INTERNAL</code>.</p>

#### `editor_error_name` {#function-editor-error-name}

<div class="function-declaration" id="function-editor-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view editor_error_name(
    <a href="editor-error.html#symbol-protocol-error-code">protocol_error_code</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name for the error category (same text as <code>to_string</code>).</p>

#### `parse_error_code` {#function-parse-error-code}

<div class="function-declaration" id="function-parse-error-code-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="editor-error.html#symbol-protocol-error-code">protocol_error_code</a>&gt; parse_error_code(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a wire spelling (exact, case-sensitive). nullopt for anything else, including &quot;<code>NONE</code>&quot;.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="editor-error.html#symbol-protocol-error-code">protocol_error_code</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire spelling of a code (&quot;<code>ENTITY_NOT_FOUND</code>&quot;); &quot;&quot; for values outside the enumeration.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="all_error_codes"></a>
<a id="classes"></a>
<a id="classify_error"></a>
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_error_name"></a>
<a id="extension-points"></a>
<a id="function-make-editor-error"></a>
<a id="function-make-editor-error-1"></a>
<a id="internals-and-algorithms"></a>
<a id="make_editor_error"></a>
<a id="other-declarations"></a>
<a id="parse_error_code"></a>
<a id="protocol_error_code"></a>
<a id="protocol_error_codeasset_not_found"></a>
<a id="protocol_error_codebusy"></a>
<a id="protocol_error_codecancelled"></a>
<a id="protocol_error_codecompile_failed"></a>
<a id="protocol_error_codecomponent_not_found"></a>
<a id="protocol_error_codecount"></a>
<a id="protocol_error_codeentity_not_found"></a>
<a id="protocol_error_codeinternal"></a>
<a id="protocol_error_codeinvalid_args"></a>
<a id="protocol_error_codeinvalid_request"></a>
<a id="protocol_error_codeio_error"></a>
<a id="protocol_error_codemessage_too_large"></a>
<a id="protocol_error_codenone"></a>
<a id="protocol_error_codenot_available"></a>
<a id="protocol_error_codeparse_error"></a>
<a id="protocol_error_codepath_not_found"></a>
<a id="protocol_error_codeproject_not_open"></a>
<a id="protocol_error_codeproject_not_trusted"></a>
<a id="protocol_error_codescene_not_open"></a>
<a id="protocol_error_codetimeout"></a>
<a id="protocol_error_codetype_not_found"></a>
<a id="protocol_error_codeunauthorised"></a>
<a id="protocol_error_codeunknown_command"></a>
<a id="protocol_error_codewrong_play_state"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

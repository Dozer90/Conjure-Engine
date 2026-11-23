---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# command_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/command_registry.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The command catalogue: descriptors by name with their compiled argument schemas, the self-description <code>editor.schema</code> returns and the hash <code>session.hello</code> reports. Main thread only; <code>module.load</code> adds commands at runtime and <code>module.unload</code> removes them by owner.</p>

## Structs

| File | Description |
|---|---|
| [`command_registry::entry`](command-registry/command-registry-entry.md) |  |

## Classes

| File | Description |
|---|---|
| [`command_registry`](command-registry/command-registry.md) |  |

## Functions

### Public

#### `protocol_event_names` {#function-protocol-event-names}

<div class="function-declaration" id="function-protocol-event-names-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">const</span> std::string_view&gt; protocol_event_names() <span class="k">noexcept</span>;</code></pre></div>

<p>The event names the broadcaster emits, in a fixed order (part of <code>editor.schema</code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="command_registry"></a>
<a id="command_registryadd"></a>
<a id="command_registryargs_schema"></a>
<a id="command_registryclosest_names"></a>
<a id="command_registrycommand_registry"></a>
<a id="command_registrycommand_registry_1"></a>
<a id="command_registrydescribe"></a>
<a id="command_registrydescribe_all"></a>
<a id="command_registrydescriptors"></a>
<a id="command_registryentry"></a>
<a id="command_registryentryargsschema"></a>
<a id="command_registryentrydescriptor"></a>
<a id="command_registryentryresultschema"></a>
<a id="command_registryfind"></a>
<a id="command_registryfind_entry"></a>
<a id="command_registrymentries"></a>
<a id="command_registrymschemahash"></a>
<a id="command_registrymschemahashstale"></a>
<a id="command_registrynames"></a>
<a id="command_registryoperator"></a>
<a id="command_registryrecompute_hash"></a>
<a id="command_registryremove"></a>
<a id="command_registryremove_owned_by"></a>
<a id="command_registryschema_hash"></a>
<a id="command_registrysize"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="protocol_event_names"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

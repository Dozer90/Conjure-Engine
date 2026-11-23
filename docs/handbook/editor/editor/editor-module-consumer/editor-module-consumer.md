---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [editor_module_consumer.h](../editor-module-consumer.md)

# editor_module_consumer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_module_consumer.h` |
| Source File Path | `engine/src/editor/core/editor_module_consumer.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API editor_module_consumer final : public modules::module_consumer
```

## Inheritance Hierarchy


[`modules::module_consumer`](../../../engine/modules/module-consumer/module-consumer.md) → `editor_module_consumer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">editor_module_consumer(
    <a href="../command-registry/command-registry.html">command_registry</a> &amp;registry,
    <span class="k">const</span> <a href="../play-controller/play-controller.html">play_controller</a> *play);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~editor_module_consumer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAttached</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../../../engine/modules/module-types/module-id.html">modules::<wbr>module_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mModuleNames</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Panels<wbr>Changed</code></td><td><code class="signature"><a href="../../../engine/core/event/event.html">core::event</a>&lt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPanels</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="module-panel-entry.html">module_<wbr>panel_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPlay</code></td><td><code class="signature"><span class="k">const</span> <a href="../play-controller/play-controller.html">play_controller</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><a href="../command-registry/command-registry.html">command_registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `attach` {#function-attach}

<div class="function-declaration" id="function-attach-1"><pre><code><span class="k">void</span> attach(
    <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a> id,
    <a href="../../../engine/modules/module-context/module-context.html">modules::module_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../engine/modules/module-registration-set/module-registration-set.html">modules::module_registration_set</a> &amp;set) <span class="k">override</span>;</code></pre></div>

#### `attached` {#function-attached}

<div class="function-declaration" id="function-attached-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a>&gt; attached() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Modules whose records are currently applied.</p>

#### `can_detach` {#function-can-detach}

<div class="function-declaration" id="function-can-detach-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; can_detach(
    <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a> id,
    <a href="../../../engine/modules/module-types.html#symbol-unload-policy">modules::unload_policy</a> policy) <span class="k">override</span>;</code></pre></div>

#### `detach` {#function-detach}

<div class="function-declaration" id="function-detach-1"><pre><code><span class="k">void</span> detach(
    <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a> id,
    <a href="../../../engine/modules/module-consumer/reload-context.html">modules::reload_context</a> *reload) <span class="k">override</span>;</code></pre></div>

#### `live_use_count` {#function-live-use-count}

<div class="function-declaration" id="function-live-use-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> live_use_count(
    <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a> id) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/string-id/string-id.html">core::string_id</a> name() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `on_panels_changed` {#function-on-panels-changed}

<div class="function-declaration" id="function-on-panels-changed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/event/event.html">core::event</a>&lt;&gt; &amp;on_panels_changed() <span class="k">noexcept</span>;</code></pre></div>

<p>Emitted after attach or detach changed the panel list.</p>

#### `panels` {#function-panels}

<div class="function-declaration" id="function-panels-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="module-panel-entry.html">module_panel_entry</a>&gt; panels() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The panels of every attached module, in attach order.</p>

#### `restore` {#function-restore}

<div class="function-declaration" id="function-restore-1"><pre><code><span class="k">void</span> restore(
    <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a> id,
    <span class="k">const</span> <a href="../../../engine/modules/module-consumer/reload-context.html">modules::reload_context</a> &amp;reload) <span class="k">override</span>;</code></pre></div>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate(
    <a href="../../../engine/modules/module-types/module-id.html">modules::module_id</a> id,
    <span class="k">const</span> <a href="../../../engine/modules/module-registration-set/module-registration-set.html">modules::module_registration_set</a> &amp;set,
    <span class="k">const</span> <a href="../../../engine/modules/module-registry/module-registry.html">modules::module_registry</a> &amp;registry) <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

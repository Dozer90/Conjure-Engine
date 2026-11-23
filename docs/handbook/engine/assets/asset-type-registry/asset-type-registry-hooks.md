---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_type_registry.h](../asset-type-registry.md)

# asset_type_registry_hooks
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_type_registry.h` |
| Source File Path | `engine/src/assets/asset_type_registry.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
struct asset_type_registry_hooks
```









## Functions

### Public

#### `bool` {#function-bool}

<div class="function-declaration" id="function-bool-1"><pre><code>std::function&lt;<span class="k">bool</span>(
    asset_type_id)&gt; isInUse =;</code></pre></div>

#### `void` {#function-void}

<div class="function-declaration" id="function-void-1"><pre><code>std::function&lt;<span class="k">void</span>(
    <span class="k">const</span> <a href="asset-type-desc.html">asset_type_desc</a> &amp;)&gt; onRegistered =;</code></pre></div>

<div class="function-declaration" id="function-void-2"><pre><code>std::function&lt;<span class="k">void</span>(
    asset_type_id)&gt; onUnregistered =;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

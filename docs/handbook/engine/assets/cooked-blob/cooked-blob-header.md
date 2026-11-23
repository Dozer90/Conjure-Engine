---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [cooked_blob.h](../cooked-blob.md)

# cooked_blob_header
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/cooked_blob.h` |
| Source File Path | `engine/src/assets/cooked_blob.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md) |

## Syntax

```cpp
struct cooked_blob_header
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dependency<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">COOKED_<wbr>BLOB_<wbr>FORMAT_<wbr>VERSION</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">importer<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAGIC</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr><span class="k">char</span>,<wbr> <span class="mi">4</span>&gt;</code></td><td><code class="signature">{<span class="s">'C'</span>,<wbr> <span class="s">'J'</span>,<wbr> <span class="s">'A'</span>,<wbr> <span class="s">'B'</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">PAYLOAD_<wbr>ALIGNMENT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">16</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloadHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloadSize</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">settingsHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">SIZE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">80</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sourceHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a></code></td><td><code class="signature">asset_<wbr>type_<wbr>id::<wbr>INVALID</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">typeVersion</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `payload_offset` {#function-payload-offset}

<div class="function-declaration" id="function-payload-offset-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u64">u64</a> payload_offset() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Offset of the payload from the start of the blob: the header and the dependency list rounded up to 16 bytes.</p>

#### `total_size` {#function-total-size}

<div class="function-declaration" id="function-total-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u64">u64</a> total_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Total size of the serialised blob. Only meaningful for a consistent header (one built by <code>make_cooked_blob</code> or <code>write_cooked_blob</code>); validation of untrusted bytes compares the parts separately so a corrupt <code>payloadSize</code> cannot wrap the sum.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

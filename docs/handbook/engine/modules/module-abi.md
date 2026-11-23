---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_abi.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_abi.h` |
| API ↗ | [Loading and managing modules](../../../api/modules/loading-and-managing.md) |

<p>The <code>C</code> front door of a shared module.</p>

## Structs

| File | Description |
|---|---|
| [`conjure_module_abi_signature`](module-abi/conjure-module-abi-signature.md) | Everything that must match exactly between a module binary and the host before any <code>C</code>++ object crosses the boundary. Pure <code>C</code> layout; compared field by field by <code>compare_abi()</code>. |
| [`conjure_module_info`](module-abi/conjure-module-info.md) | What <code>conjure_module_describe()</code> returns: the signature plus the module name, which must equal the manifest name. |

## Type Aliases

### `conjure_module_describe_fn` {#symbol-conjure-module-describe-fn}

```cpp
typedef const conjure_module_info *(*conjure_module_describe_fn)(void);
```

### `conjure_module_entry_fn` {#symbol-conjure-module-entry-fn}

```cpp
typedef std::uint32_t (*conjure_module_entry_fn)(conjure::modules::module_context *context);
```

## Forward Declarations

### `module_context` {#symbol-module-context}

```cpp
class module_context;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ABI_ARCH_ARM64"></a><code>ABI_ARCH_ARM64</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">2</span></code> |  |
| <a id="variable-ABI_ARCH_X64"></a><code>ABI_ARCH_X64</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-ABI_COMPILER_APPLECLANG"></a><code>ABI_COMPILER_APPLECLANG</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">4</span></code> |  |
| <a id="variable-ABI_COMPILER_CLANG"></a><code>ABI_COMPILER_CLANG</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">3</span></code> |  |
| <a id="variable-ABI_COMPILER_GCC"></a><code>ABI_COMPILER_GCC</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">2</span></code> |  |
| <a id="variable-ABI_COMPILER_MSVC"></a><code>ABI_COMPILER_MSVC</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-ABI_RUNTIME_ASAN"></a><code>ABI_RUNTIME_ASAN</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">1</span>u &lt;&lt; <span class="mi">1</span></code> |  |
| <a id="variable-ABI_RUNTIME_DYNAMIC_CRT"></a><code>ABI_RUNTIME_DYNAMIC_CRT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">1</span>u &lt;&lt; <span class="mi">0</span></code> |  |
| <a id="variable-ABI_RUNTIME_UBSAN"></a><code>ABI_RUNTIME_UBSAN</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">1</span>u &lt;&lt; <span class="mi">2</span></code> |  |
| <a id="variable-ABI_SIGNATURE_COMPILER_MAJOR"></a><code>ABI_SIGNATURE_COMPILER_MAJOR</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature">static_<wbr>cast&lt;<wbr>std::<wbr>uint32_<wbr>t&gt;(<span class="na">CONJURE_<wbr>ABI_<wbr>SIGNATURE_<wbr>COMPILER_<wbr>MAJOR</span>)</code> | The compiler major the signature of this translation unit reports (see the header comment). |
| <a id="variable-ABI_SIGNATURE_RUNTIME_FLAGS"></a><code>ABI_SIGNATURE_RUNTIME_FLAGS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature">((<span class="na">CONJURE_<wbr>ABI_<wbr>SIGNATURE_<wbr>DYNAMIC_<wbr>CRT</span>) != <span class="mi">0</span> ? ABI_<wbr>RUNTIME_<wbr>DYNAMIC_<wbr>CRT : <span class="mi">0</span>u) \| ((<span class="na">CONJURE_<wbr>ABI_<wbr>SIGNATURE_<wbr>ASAN</span>) != <span class="mi">0</span> ? ABI_<wbr>RUNTIME_<wbr>ASAN : <span class="mi">0</span>u) \| ((<span class="na">CONJURE_<wbr>ABI_<wbr>SIGNATURE_<wbr>UBSAN</span>) != <span class="mi">0</span> ? ABI_<wbr>RUNTIME_<wbr>UBSAN : <span class="mi">0</span>u)</code> | The runtime flags the signature of this translation unit reports: bit0 dynamic CRT, bit1 ASan, bit2 UBSan. |
| <a id="variable-ABI_STDLIB_LIBCXX"></a><code>ABI_STDLIB_LIBCXX</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">3</span></code> |  |
| <a id="variable-ABI_STDLIB_LIBSTDCXX"></a><code>ABI_STDLIB_LIBSTDCXX</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">2</span></code> |  |
| <a id="variable-ABI_STDLIB_MSVC"></a><code>ABI_STDLIB_MSVC</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-ABI_STDLIB_UNKNOWN"></a><code>ABI_STDLIB_UNKNOWN</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>uint32_<wbr>t</code> | <code class="signature"><span class="mi">0</span></code> |  |

## Functions

### Public

#### `compare_abi` {#function-compare-abi}

<div class="function-declaration" id="function-compare-abi-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; compare_abi(
    <span class="k">const</span> <a href="module-abi/conjure-module-abi-signature.html">conjure_module_abi_signature</a> &amp;moduleSignature,
    <span class="k">const</span> <a href="module-abi/conjure-module-abi-signature.html">conjure_module_abi_signature</a> &amp;hostSignature);</code></pre></div>

<p>Compares every field, <code>structSize</code> first (a smaller struct is rejected before any other field is read), then magic, <code>abiVersion</code> and the rest in declaration order. <code>ABI_MISMATCH</code> names the first differing field with the expected and actual values and asks for a rebuild.</p>

#### `host_abi_signature` {#function-host-abi-signature}

<div class="function-declaration" id="function-host-abi-signature-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">const</span> <a href="module-abi/conjure-module-abi-signature.html">conjure_module_abi_signature</a> &amp;host_abi_signature() <span class="k">noexcept</span>;</code></pre></div>

<p>The signature the engine itself was built with.</p>

#### `make_abi_signature` {#function-make-abi-signature}

<div class="function-declaration" id="function-make-abi-signature-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-abi/conjure-module-abi-signature.html">conjure_module_abi_signature</a> make_abi_signature() <span class="k">noexcept</span>;</code></pre></div>

<p>The signature of the translation unit that calls it. <code>CONJURE_TEST_ABI_VERSION_OVERRIDE</code> and <code>CONJURE_TEST_ENGINE_VERSION_OVERRIDE</code> (a major version) exist for the mismatch fixtures only.</p>

#### `make_module_info` {#function-make-module-info}

<div class="function-declaration" id="function-make-module-info-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-abi/conjure-module-info.html">conjure_module_info</a> make_module_info(
    <span class="k">const</span> <span class="k">char</span> *name) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>describe()</code> payload of a module named <code>name</code> (a string literal that outlives the binary).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="abi_arch_arm64"></a>
<a id="abi_arch_x64"></a>
<a id="abi_compiler_appleclang"></a>
<a id="abi_compiler_clang"></a>
<a id="abi_compiler_gcc"></a>
<a id="abi_compiler_msvc"></a>
<a id="abi_runtime_asan"></a>
<a id="abi_runtime_dynamic_crt"></a>
<a id="abi_runtime_ubsan"></a>
<a id="abi_signature_compiler_major"></a>
<a id="abi_signature_runtime_flags"></a>
<a id="abi_stdlib_libcxx"></a>
<a id="abi_stdlib_libstdcxx"></a>
<a id="abi_stdlib_msvc"></a>
<a id="abi_stdlib_unknown"></a>
<a id="classes"></a>
<a id="compare_abi"></a>
<a id="conjure_module_abi_signature"></a>
<a id="conjure_module_abi_signatureabiversion"></a>
<a id="conjure_module_abi_signaturearchitecture"></a>
<a id="conjure_module_abi_signaturebuildconfig"></a>
<a id="conjure_module_abi_signaturecompilerid"></a>
<a id="conjure_module_abi_signaturecompilermajor"></a>
<a id="conjure_module_abi_signatureengineversionmajor"></a>
<a id="conjure_module_abi_signatureengineversionminor"></a>
<a id="conjure_module_abi_signatureengineversionpatch"></a>
<a id="conjure_module_abi_signaturefeatureflags"></a>
<a id="conjure_module_abi_signatureheaderhash"></a>
<a id="conjure_module_abi_signaturemagic"></a>
<a id="conjure_module_abi_signatureruntimeflags"></a>
<a id="conjure_module_abi_signaturestandardlibraryid"></a>
<a id="conjure_module_abi_signaturestructsize"></a>
<a id="conjure_module_describe_fn"></a>
<a id="conjure_module_entry_fn"></a>
<a id="conjure_module_info"></a>
<a id="conjure_module_infoabi"></a>
<a id="conjure_module_infoname"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="host_abi_signature"></a>
<a id="internals-and-algorithms"></a>
<a id="make_abi_signature"></a>
<a id="make_module_info"></a>
<a id="module_context"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

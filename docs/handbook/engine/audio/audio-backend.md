---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# audio_backend.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/audio_backend.h` |
| API ↗ | [Clips and decoding](../../../api/audio/clips-and-decoding.md) |

<p>The backend interface: the piece that owns a device (or a virtual clock) and drives the mix callback. Implementations: miniaudio (real devices, device thread) and <code>null</code> (headless, main thread). Every method is main-thread only except <code>state()</code>.</p>



## Classes

| File | Description |
|---|---|
| [`audio_backend`](audio-backend/audio-backend.md) |  |

## Functions

### Public

#### `create_backend` {#function-create-backend}

<div class="function-declaration" id="function-create-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="audio-backend/audio-backend.html">audio_backend</a>&gt;&gt; create_backend(
    <a href="audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a> kind);</code></pre></div>

<p>Creates a stopped backend. <code>AUTO</code> = <code>MINIAUDIO</code> when a device backend is compiled in, else <code>NULL_BACKEND</code>. Fails with <code>UNSUPPORTED</code> (core) when the requested kind is not compiled in.</p>

#### `is_backend_compiled` {#function-is-backend-compiled}

<div class="function-declaration" id="function-is-backend-compiled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_backend_compiled(
    <a href="audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>True when a backend of this kind is compiled into the library (<code>AUTO</code> and <code>NULL_BACKEND</code> always).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audio_backend"></a>
<a id="audio_backendaudio_backend"></a>
<a id="audio_backendaudio_backend_1"></a>
<a id="audio_backendenumerate_devices"></a>
<a id="audio_backendkind"></a>
<a id="audio_backendoperator"></a>
<a id="audio_backendoutput_format"></a>
<a id="audio_backendstart"></a>
<a id="audio_backendstate"></a>
<a id="audio_backendstop"></a>
<a id="audio_backendupdate"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="create_backend"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_backend_compiled"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

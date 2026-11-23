---
kind: header
---

[Editor](../../editor.md) → [User Interface](../user-interface.md)

# screenshot_service.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/screenshot_service.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Screenshots: <code>view.screenshot</code> parks its request, the service waits the asked number of frames, captures an RGBA8 image through its backend on the main thread, then hands the image to a <code>core::jobs</code> job that encodes the file (<code>PNG</code>, or lossless JPEG XL through the renderer&#x27;s encoder) and writes it (a 16384 x 16384 capture must not stall the frame); the job posts the completion back to the main thread, which answers {path, width, height, frame, format}. Without a job system the encode runs inline. The renderer backend draws the scene through the requested camera (&quot;editor&quot;, &quot;game&quot; or a camera entity) into a render target of the requested size and captures it through <code>renderer::read_back</code> once a frame has been rendered into it; on the <code>null</code> RHI (every headless run) there is no backend and the request fails with <code>NOT_AVAILABLE</code>. Tests install a memory backend that paints a solid colour.</p>

## Structs

| File | Description |
|---|---|
| [`screenshot_request`](screenshot-service/screenshot-request.md) |  |
| [`screenshot_service::pending`](screenshot-service/screenshot-service-pending.md) |  |

## Classes

| File | Description |
|---|---|
| [`memory_screenshot_backend`](screenshot-service/memory-screenshot-backend.md) | <code>A</code> backend that paints every pixel one colour (tests, documentation runs). |
| [`screenshot_backend`](screenshot-service/screenshot-backend.md) | Produces the pixels. <code>begin()</code> runs when the request is accepted and arranges for the picture to be drawn in the frame being built; <code>prepare_frame()</code> runs on every later frame the request still waits, to have it drawn again; <code>frame_rendered()</code> says whether a frame has been drawn since it was last asked; <code>capture()</code> runs once enough frames were. |
| [`screenshot_service`](screenshot-service/screenshot-service.md) |  |

## Enumerations

### `screenshot_format` {#symbol-screenshot-format}

```cpp
enum class screenshot_format : u8 {
    PNG = 0,
    JPEG_XL
};
```

The file a screenshot is written as. <code>PNG</code> is the default: golden images, review tools and every viewer read it. <code>JPEG_XL</code> is a lossless JPEG XL file (the same pixels, usually a smaller file), encoded by the renderer module, so a build without the renderer cannot write it.

## Forward Declarations

### `editor_context` {#symbol-editor-context}

```cpp
class editor_context;
```

## Functions

### Public

#### `parse_screenshot_format` {#function-parse-screenshot-format}

<div class="function-declaration" id="function-parse-screenshot-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="screenshot-service.html#symbol-screenshot-format">screenshot_format</a>&gt; parse_screenshot_format(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a wire name (&quot;png&quot; or &quot;jxl&quot;, exactly); nullopt for anything else.</p>

#### `screenshot_format_of` {#function-screenshot-format-of}

<div class="function-declaration" id="function-screenshot-format-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="screenshot-service.html#symbol-screenshot-format">screenshot_format</a>&gt; screenshot_format_of(
    <span class="k">const</span> std::filesystem::path &amp;path);</code></pre></div>

<p>The format a path&#x27;s extension names: &quot;.png&quot; or &quot;.jxl&quot; in any case; nullopt for any other extension or none.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="screenshot-service.html#symbol-screenshot-format">screenshot_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire name of a format: &quot;png&quot; or &quot;jxl&quot;.</p>

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
<a id="editor_context"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="memory_screenshot_backend"></a>
<a id="memory_screenshot_backendbegin"></a>
<a id="memory_screenshot_backendbegun"></a>
<a id="memory_screenshot_backendcapture"></a>
<a id="memory_screenshot_backendcaptured"></a>
<a id="memory_screenshot_backendmbegun"></a>
<a id="memory_screenshot_backendmcaptured"></a>
<a id="memory_screenshot_backendmcolour"></a>
<a id="memory_screenshot_backendmemory_screenshot_backend"></a>
<a id="memory_screenshot_backendmemory_screenshot_backend_1"></a>
<a id="other-declarations"></a>
<a id="parse_screenshot_format"></a>
<a id="screenshot_backend"></a>
<a id="screenshot_backendabandon"></a>
<a id="screenshot_backendbegin"></a>
<a id="screenshot_backendcapture"></a>
<a id="screenshot_backendframe_rendered"></a>
<a id="screenshot_backendoperator"></a>
<a id="screenshot_backendprepare_frame"></a>
<a id="screenshot_backendscreenshot_backend"></a>
<a id="screenshot_backendscreenshot_backend_1"></a>
<a id="screenshot_format"></a>
<a id="screenshot_format_of"></a>
<a id="screenshot_formatjpeg_xl"></a>
<a id="screenshot_formatpng"></a>
<a id="screenshot_request"></a>
<a id="screenshot_requestcamera"></a>
<a id="screenshot_requestformat"></a>
<a id="screenshot_requestheight"></a>
<a id="screenshot_requestpath"></a>
<a id="screenshot_requestwaitframes"></a>
<a id="screenshot_requestwidth"></a>
<a id="screenshot_service"></a>
<a id="screenshot_serviceavailable"></a>
<a id="screenshot_servicecancel_pending"></a>
<a id="screenshot_servicecompleted_count"></a>
<a id="screenshot_servicecreate_renderer_backend"></a>
<a id="screenshot_serviceencode_and_complete"></a>
<a id="screenshot_serviceencoding_count"></a>
<a id="screenshot_servicefinish"></a>
<a id="screenshot_servicemalive"></a>
<a id="screenshot_servicembackend"></a>
<a id="screenshot_servicemcompleted"></a>
<a id="screenshot_servicemcontext"></a>
<a id="screenshot_servicemencoding"></a>
<a id="screenshot_servicempending"></a>
<a id="screenshot_serviceoperator"></a>
<a id="screenshot_serviceparse_request"></a>
<a id="screenshot_servicepending"></a>
<a id="screenshot_servicepending_count"></a>
<a id="screenshot_servicependingbegun"></a>
<a id="screenshot_servicependingframesleft"></a>
<a id="screenshot_servicependingreply"></a>
<a id="screenshot_servicependingrequest"></a>
<a id="screenshot_servicerequest"></a>
<a id="screenshot_servicescreenshot_service"></a>
<a id="screenshot_servicescreenshot_service_1"></a>
<a id="screenshot_serviceset_backend"></a>
<a id="screenshot_servicesupports"></a>
<a id="screenshot_servicetick"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

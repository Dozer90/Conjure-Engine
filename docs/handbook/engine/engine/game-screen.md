---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# game_screen.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/game_screen.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md) |

<p>The screen a game draws to, as <code>game_screen_of</code> reports it.</p>

## Structs

| File | Description |
|---|---|
| [`game_screen`](game-screen/game-screen.md) | The screen a game draws to, as <code>game_screen_of</code> reports it. |

## Functions

### Public

#### `game_screen_of` {#function-game-screen-of}

<div class="function-declaration" id="function-game-screen-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="game-screen/game-screen.html">game_screen</a> game_screen_of(
    <span class="k">const</span> <a href="../platform/window/window.html">platform::window</a> *window) <span class="k">noexcept</span>;</code></pre></div>

<p>The screen of <code>window</code>: its framebuffer in pixels (0 x 0 while minimised), <code>content_scale()</code>, the UI scale of the monitor under the window centre (<code>position()</code> + <code>window_size()</code> / 2, see <code>monitor_ui_scale</code>) and the focus, minimised and fullscreen states. <code>A</code> <code>null</code> window gives a default <code><a href="game-screen/game-screen.html">game_screen</a></code> (0 x 0, scales 1, every state <code>false</code>). On a backend that cannot learn the window position (glfw on Wayland), <code>position()</code> is the last one requested, so the UI scale can be another monitor&#x27;s than the one showing the window. Main thread only, like every window call.</p>

#### `monitor_ui_scale` {#function-monitor-ui-scale}

<div class="function-declaration" id="function-monitor-ui-scale-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/types.html#symbol-f32">f32</a> monitor_ui_scale(
    std::span&lt;<span class="k">const</span> <a href="../platform/monitor/monitor-info.html">platform::monitor_info</a>&gt; monitors,
    <a href="../core/math/glm-types.html#symbol-ivec2">math::ivec2</a> point) <span class="k">noexcept</span>;</code></pre></div>

<p>The UI scale (<code>contentScale.y</code>) of the first monitor whose bounds contain <code>point</code>: the rectangle from <code>monitor_info::position</code> spanning currentMode&#x27;s width and height, in the virtual desktop coordinates that <code>window::position()</code> uses (screen coordinates: pixels, points on macOS). The bounds are used, not the work area, because a backend clamps a negative work area origin to 0 (a monitor left of or above the primary would then cover the wrong place) and a point over a task bar still belongs to its monitor. <code>A</code> monitor whose current mode is 0 x 0 contains nothing. When no monitor contains the point, the primary monitor&#x27;s scale (the first monitor&#x27;s when none is marked primary); 1 when there are no monitors or the scale found is not a positive finite number.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

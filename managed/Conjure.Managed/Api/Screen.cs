using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The game screen, the surface the game draws to, read straight from the shared frame block (no native call):
/// the window framebuffer in pixels, written by the engine before every frame. Camera picking and Screen sprites
/// use the same pixels (origin top-left, y down). In the editor's play mode the screen is the editor window.
/// Before the native handshake every value is the one of a missing window: 0 x 0, scales 1, every state false.
/// </summary>
public static unsafe class Screen
{
    private static FrameBlock* Frame => NativeBridge.Frame;

    /// <summary>
    /// The width in pixels; 0 while the window is minimised or when there is none.
    /// </summary>
    public static int Width => Frame == null ? 0 : (int)Frame->ScreenWidth;

    /// <summary>
    /// The height in pixels; 0 while the window is minimised or when there is none.
    /// </summary>
    public static int Height => Frame == null ? 0 : (int)Frame->ScreenHeight;

    /// <summary>
    /// Width and Height in pixels.
    /// </summary>
    public static Vector2 Size => new Vector2(Width, Height);

    /// <summary>
    /// Width / Height; 0 when Height is 0.
    /// </summary>
    public static float AspectRatio
    {
        get
        {
            int height = Height;
            return height == 0 ? 0.0f : (float)Width / height;
        }
    }

    /// <summary>
    /// Framebuffer pixels per logical window unit, per axis (platform window content_scale): 1 on Windows and X11
    /// whatever the DPI; the backing scale on a macOS Retina display or a scaled Wayland output; the last value
    /// while minimised. Input positions are logical; LogicalToPixels converts them.
    /// </summary>
    public static Vector2 ContentScale =>
        Frame == null ? Vector2.One : new Vector2(Frame->ContentScaleX, Frame->ContentScaleY);

    /// <summary>
    /// The OS UI scale of the monitor under the window centre (DPI / 96 on Windows: 1.5 at 144 DPI), for sizing
    /// text and HUDs. When no monitor contains the centre, the primary monitor's scale (the first monitor's when
    /// none is primary); 1 when the platform reports no monitors or a scale that is not a positive number. On
    /// Wayland the window position is not known, so the monitor can be another one than the one showing the window.
    /// </summary>
    public static float UiScale => Frame == null ? 1.0f : Frame->UiScale;

    /// <summary>
    /// True while the window has the input focus.
    /// </summary>
    public static bool IsFocused => Frame != null && (Frame->WindowState & FrameBlock.WindowFocused) != 0;

    /// <summary>
    /// True while the window is minimised (the screen is then 0 x 0).
    /// </summary>
    public static bool IsMinimised => Frame != null && (Frame->WindowState & FrameBlock.WindowMinimised) != 0;

    /// <summary>
    /// True while the window is borderless fullscreen.
    /// </summary>
    public static bool IsFullscreen => Frame != null && (Frame->WindowState & FrameBlock.WindowFullscreen) != 0;

    /// <summary>
    /// A position in logical window units (Input.MousePosition) in pixels: logical * ContentScale.
    /// </summary>
    public static Vector2 LogicalToPixels(Vector2 logical) => logical * ContentScale;
}

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Conjure.Interop;

/// <summary>
/// Mirror of conjure_gamepad_state (40 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct GamepadState
{
    public uint ButtonsDown;
    public uint ButtonsPressed;
    public uint ButtonsReleased;
    public fixed float Axes[6];
    public byte Connected;
}

/// <summary>
/// The four gamepad records of the input block, laid out back to back.
/// </summary>
[InlineArray(4)]
public struct GamepadStates
{
    private GamepadState _element;
}

/// <summary>
/// Mirror of conjure_input_block (392 bytes): the full input snapshot of the frame (D4).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct InputBlock
{
    public fixed ulong KeysDown[8];
    public fixed ulong KeysPressed[8];
    public fixed ulong KeysReleased[8];
    public float MouseX;
    public float MouseY;
    public float MouseDx;
    public float MouseDy;
    public float ScrollX;
    public float ScrollY;
    public uint MouseButtonsDown;
    public uint MouseButtonsPressed;
    public uint MouseButtonsReleased;
    public GamepadStates Gamepads;
}

/// <summary>
/// Mirror of conjure_frame_block (512 bytes): written by the engine before FRAME_BEGIN, read by Time, Input and
/// Screen. The screen fields are the game screen in pixels (0 x 0 while minimised or without a window), the
/// framebuffer / logical ratio, the OS UI scale of the window's monitor and the WindowState bits. Native pins the
/// layout with static_asserts; Initialize checks the size and the offsets below.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = ExpectedSize)]
public struct FrameBlock
{
    public const int ExpectedSize = 512;
    public const int ExpectedFrameIndexOffset = 32;
    public const int ExpectedInputOffset = 48;
    public const int ExpectedInputSize = 392;
    public const int ExpectedScreenWidthOffset = 440;
    public const int ExpectedUiScaleOffset = 456;
    public const int ExpectedWindowStateOffset = 460;

    /// <summary>
    /// WindowState bit: the window has the input focus (FRAME_WINDOW_FOCUSED).
    /// </summary>
    public const byte WindowFocused = 1;

    /// <summary>
    /// WindowState bit: the window is minimised, so the screen is 0 x 0 (FRAME_WINDOW_MINIMISED).
    /// </summary>
    public const byte WindowMinimised = 2;

    /// <summary>
    /// WindowState bit: the window is borderless fullscreen (FRAME_WINDOW_FULLSCREEN).
    /// </summary>
    public const byte WindowFullscreen = 4;

    public double TimeSeconds;
    public double UnscaledTimeSeconds;
    public float DeltaTime;
    public float FixedDeltaTime;
    public float TimeScale;
    public ulong FrameIndex;
    public uint FixedStepIndex;
    public byte CurrentPhase;
    public byte InPlayMode;
    public InputBlock Input;
    public uint ScreenWidth;
    public uint ScreenHeight;
    public float ContentScaleX;
    public float ContentScaleY;
    public float UiScale;
    public byte WindowState;
}

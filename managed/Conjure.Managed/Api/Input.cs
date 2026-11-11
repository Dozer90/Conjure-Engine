using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The input snapshot of the frame, read straight from the shared frame block (D4): no native call per
/// query. Key codes follow GLFW numbering (platform::key).
/// </summary>
public static unsafe partial class Input
{
    public const int GamepadCount = 4;

    private static FrameBlock* Frame => NativeBridge.Frame;

    public static bool IsKeyDown(Key key) => Test(Frame == null ? null : Frame->Input.KeysDown, (int)key);

    public static bool IsKeyPressed(Key key) => Test(Frame == null ? null : Frame->Input.KeysPressed, (int)key);

    public static bool IsKeyReleased(Key key) => Test(Frame == null ? null : Frame->Input.KeysReleased, (int)key);

    public static Vector2 MousePosition => Frame == null ? Vector2.Zero : new Vector2(Frame->Input.MouseX, Frame->Input.MouseY);

    public static Vector2 MouseDelta => Frame == null ? Vector2.Zero : new Vector2(Frame->Input.MouseDx, Frame->Input.MouseDy);

    public static Vector2 ScrollDelta => Frame == null ? Vector2.Zero : new Vector2(Frame->Input.ScrollX, Frame->Input.ScrollY);

    public static bool IsMouseButtonDown(MouseButton button) => Frame != null && (Frame->Input.MouseButtonsDown & (1u << (int)button)) != 0;

    public static bool IsMouseButtonPressed(MouseButton button) => Frame != null && (Frame->Input.MouseButtonsPressed & (1u << (int)button)) != 0;

    public static bool IsMouseButtonReleased(MouseButton button) => Frame != null && (Frame->Input.MouseButtonsReleased & (1u << (int)button)) != 0;

    public static bool IsGamepadConnected(int index) => ValidGamepad(index) && Frame->Input.Gamepads[index].Connected != 0;

    public static bool IsGamepadButtonDown(int index, GamepadButton button) =>
        ValidGamepad(index) && (Frame->Input.Gamepads[index].ButtonsDown & (1u << (int)button)) != 0;

    public static bool IsGamepadButtonPressed(int index, GamepadButton button) =>
        ValidGamepad(index) && (Frame->Input.Gamepads[index].ButtonsPressed & (1u << (int)button)) != 0;

    public static bool IsGamepadButtonReleased(int index, GamepadButton button) =>
        ValidGamepad(index) && (Frame->Input.Gamepads[index].ButtonsReleased & (1u << (int)button)) != 0;

    public static float GamepadAxis(int index, GamepadAxis axis)
    {
        if (!ValidGamepad(index) || (int)axis < 0 || (int)axis >= 6)
        {
            return 0.0f;
        }
        return Frame->Input.Gamepads[index].Axes[(int)axis];
    }

    private static bool ValidGamepad(int index) => Frame != null && index >= 0 && index < GamepadCount;

    private static bool Test(ulong* bits, int key)
    {
        if (bits == null || key < 0 || key >= 512)
        {
            return false;
        }
        return ((bits[key >> 6] >> (key & 63)) & 1UL) != 0;
    }
}

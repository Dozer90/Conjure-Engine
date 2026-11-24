using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The input snapshot of the frame, read straight from the shared frame block (D4): no native call per
/// query. Key codes follow GLFW numbering (platform::key). Pressed and Released are true for exactly the frame
/// the transition happened in; Down stays true from the press frame on and Held from the frame after it.
/// </summary>
public static unsafe partial class Input
{
    public const int GamepadCount = 4;

    private const int KeyCount = 512;
    private const int MouseButtonCount = 8;
    private const int GamepadButtonCount = 15;
    private const int GamepadAxisCount = 6;

    private static FrameBlock* Frame => NativeBridge.Frame;

    /// <summary>
    /// True while the key is down, including the frame it was pressed in (IsKeyHeld leaves that frame out).
    /// </summary>
    public static bool IsKeyDown(Key key) => Test(Frame == null ? null : Frame->Input.KeysDown, (int)key);

    /// <summary>
    /// True in the frame the key went down.
    /// </summary>
    public static bool IsKeyPressed(Key key) => Test(Frame == null ? null : Frame->Input.KeysPressed, (int)key);

    /// <summary>
    /// True in the frame the key came up.
    /// </summary>
    public static bool IsKeyReleased(Key key) => Test(Frame == null ? null : Frame->Input.KeysReleased, (int)key);

    /// <summary>
    /// True while the key is down and this is not the frame it was pressed in.
    /// </summary>
    public static bool IsKeyHeld(Key key) => IsKeyDown(key) && !IsKeyPressed(key);

    public static Vector2 MousePosition => Frame == null ? Vector2.Zero : new Vector2(Frame->Input.MouseX, Frame->Input.MouseY);

    public static Vector2 MouseDelta => Frame == null ? Vector2.Zero : new Vector2(Frame->Input.MouseDx, Frame->Input.MouseDy);

    public static Vector2 ScrollDelta => Frame == null ? Vector2.Zero : new Vector2(Frame->Input.ScrollX, Frame->Input.ScrollY);

    /// <summary>
    /// True while the mouse button is down, including the frame it was pressed in (IsMouseButtonHeld leaves
    /// that frame out); false for a value that names no button.
    /// </summary>
    public static bool IsMouseButtonDown(MouseButton button) =>
        Frame != null && TestButton(Frame->Input.MouseButtonsDown, (int)button, MouseButtonCount);

    /// <summary>
    /// True in the frame the mouse button went down; false for a value that names no button.
    /// </summary>
    public static bool IsMouseButtonPressed(MouseButton button) =>
        Frame != null && TestButton(Frame->Input.MouseButtonsPressed, (int)button, MouseButtonCount);

    /// <summary>
    /// True in the frame the mouse button came up; false for a value that names no button.
    /// </summary>
    public static bool IsMouseButtonReleased(MouseButton button) =>
        Frame != null && TestButton(Frame->Input.MouseButtonsReleased, (int)button, MouseButtonCount);

    /// <summary>
    /// True while the mouse button is down and this is not the frame it was pressed in; false for a value that
    /// names no button.
    /// </summary>
    public static bool IsMouseButtonHeld(MouseButton button) =>
        IsMouseButtonDown(button) && !IsMouseButtonPressed(button);

    /// <summary>
    /// True when a gamepad is connected in slot 'index' (0..3); false for any other index.
    /// </summary>
    public static bool IsGamepadConnected(int index) =>
        ValidGamepad(index) && Frame->Input.Gamepads[index].Connected != 0;

    /// <summary>
    /// True while the button of gamepad 'index' is down, including the frame it was pressed in
    /// (IsGamepadButtonHeld leaves that frame out); false for an index outside 0..3 or a value that names no
    /// button.
    /// </summary>
    public static bool IsGamepadButtonDown(int index, GamepadButton button) =>
        ValidGamepad(index) && TestButton(Frame->Input.Gamepads[index].ButtonsDown, (int)button, GamepadButtonCount);

    /// <summary>
    /// True in the frame the button of gamepad 'index' went down; false for an index outside 0..3 or a value
    /// that names no button.
    /// </summary>
    public static bool IsGamepadButtonPressed(int index, GamepadButton button) =>
        ValidGamepad(index) && TestButton(Frame->Input.Gamepads[index].ButtonsPressed, (int)button, GamepadButtonCount);

    /// <summary>
    /// True in the frame the button of gamepad 'index' came up; false for an index outside 0..3 or a value
    /// that names no button.
    /// </summary>
    public static bool IsGamepadButtonReleased(int index, GamepadButton button) =>
        ValidGamepad(index)
        && TestButton(Frame->Input.Gamepads[index].ButtonsReleased, (int)button, GamepadButtonCount);

    /// <summary>
    /// True while the button of gamepad 'index' is down and this is not the frame it was pressed in; false for
    /// an index outside 0..3 or a value that names no button.
    /// </summary>
    public static bool IsGamepadButtonHeld(int index, GamepadButton button) =>
        IsGamepadButtonDown(index, button) && !IsGamepadButtonPressed(index, button);

    /// <summary>
    /// The axis value of gamepad 'index' (sticks in [-1, 1], triggers in [0, 1]); 0 for an index outside 0..3
    /// or a value that names no axis.
    /// </summary>
    public static float GamepadAxis(int index, GamepadAxis axis)
    {
        if (!ValidGamepad(index) || (int)axis < 0 || (int)axis >= GamepadAxisCount)
        {
            return 0.0f;
        }
        return Frame->Input.Gamepads[index].Axes[(int)axis];
    }

    private static bool ValidGamepad(int index) => Frame != null && index >= 0 && index < GamepadCount;

    // C# masks a uint shift count to five bits, so without the range check (MouseButton)33 would read the bit
    // of MouseButton.Right.
    private static bool TestButton(uint mask, int button, int buttonCount) =>
        button >= 0 && button < buttonCount && (mask & (1u << button)) != 0;

    private static bool Test(ulong* bits, int key)
    {
        if (bits == null || key < 0 || key >= KeyCount)
        {
            return false;
        }
        return ((bits[key >> 6] >> (key & 63)) & 1UL) != 0;
    }
}

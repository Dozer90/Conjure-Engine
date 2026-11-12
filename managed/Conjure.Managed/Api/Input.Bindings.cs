using System;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The half of Input that goes through the input_* entries (docs/design/15_bindings.md 2.5): the cursor
/// mode, gamepad names, the clipboard and injection. Everything readable per frame stays in the frame block
/// (Input.cs, D4). An injected event shows in the next frame's snapshot, never the current one.
/// </summary>
public static unsafe partial class Input
{
    /// <summary>
    /// The cursor mode of the main window.
    /// </summary>
    public static CursorMode CursorMode
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputGetCursorMode, NativeFunction.InputGetCursorMode);
            int status = api->InputGetCursorMode();
            NativeCheck.ThrowIfFailed(status, NativeFunction.InputGetCursorMode);
            return (CursorMode)status;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputSetCursorMode, NativeFunction.InputSetCursorMode);
            NativeCheck.ThrowIfFailed(api->InputSetCursorMode((byte)value), NativeFunction.InputSetCursorMode);
        }
    }

    /// <summary>
    /// The name of the gamepad at 'index' (0..3); "" when none is connected.
    /// </summary>
    public static string GamepadName(int index)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->InputGamepadName, NativeFunction.InputGamepadName);
        string name = Utf8Marshal.ReadNeeded(index, static (int state, byte* buffer, int capacity) =>
        {
            return NativeBridge.Api->InputGamepadName(state, buffer, capacity);
        }, out int status);
        NativeCheck.ThrowIfFailed(status, NativeFunction.InputGamepadName);
        return name;
    }

    /// <summary>
    /// The system clipboard text ("" when empty).
    /// </summary>
    public static string Clipboard
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputClipboardGet, NativeFunction.InputClipboardGet);
            string text = Utf8Marshal.ReadNeeded(0, static (int state, byte* buffer, int capacity) =>
            {
                return NativeBridge.Api->InputClipboardGet(buffer, capacity);
            }, out int status);
            NativeCheck.ThrowIfFailed(status, NativeFunction.InputClipboardGet);
            return text;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputClipboardSet, NativeFunction.InputClipboardSet);
            Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
            Utf8Encoded encoded = Utf8Marshal.Encode(value, scratch);
            fixed (byte* pinned = encoded.Bytes)
            {
                NativeCheck.ThrowIfFailed(api->InputClipboardSet(pinned, encoded.Length), NativeFunction.InputClipboardSet);
            }
        }
    }

    /// <summary>
    /// Synthetic input (tests, replays, accessibility tools): each call feeds the platform input accumulator
    /// like a window event, so it is visible from the next frame on.
    /// </summary>
    public static class Inject
    {
        public static void Key(Conjure.Key key, ButtonAction action)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputInjectKey, NativeFunction.InputInjectKey);
            NativeCheck.ThrowIfFailed(api->InputInjectKey((ushort)key, (byte)action), NativeFunction.InputInjectKey);
        }

        public static void MouseButton(Conjure.MouseButton button, ButtonAction action)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputInjectMouseButton, NativeFunction.InputInjectMouseButton);
            NativeCheck.ThrowIfFailed(api->InputInjectMouseButton((byte)button, (byte)action), NativeFunction.InputInjectMouseButton);
        }

        public static void MouseMove(float x, float y)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputInjectMouseMove, NativeFunction.InputInjectMouseMove);
            NativeCheck.ThrowIfFailed(api->InputInjectMouseMove(x, y), NativeFunction.InputInjectMouseMove);
        }

        public static void GamepadButton(int index, Conjure.GamepadButton button, ButtonAction action)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputInjectGamepadButton, NativeFunction.InputInjectGamepadButton);
            NativeCheck.ThrowIfFailed(
                api->InputInjectGamepadButton(index, (byte)button, (byte)action),
                NativeFunction.InputInjectGamepadButton);
        }

        public static void GamepadAxis(int index, Conjure.GamepadAxis axis, float value)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->InputInjectGamepadAxis, NativeFunction.InputInjectGamepadAxis);
            NativeCheck.ThrowIfFailed(api->InputInjectGamepadAxis(index, (byte)axis, value), NativeFunction.InputInjectGamepadAxis);
        }
    }
}

/// <summary>
/// The cursor mode of the main window (Input.CursorMode, platform::cursor_mode): Normal shows the cursor,
/// Hidden hides it over the window, Disabled hides and locks it for mouse look, Captured keeps it visible but
/// confined to the window.
/// </summary>
public enum CursorMode : byte
{
    Normal,
    Hidden,
    Disabled,
    Captured,
}

/// <summary>
/// The action of an injected key, mouse or gamepad button event (Input.Inject, platform::button_action).
/// </summary>
public enum ButtonAction : byte
{
    Release,
    Press,
    Repeat,
}

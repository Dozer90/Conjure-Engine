namespace Conjure;

/// <summary>
/// Gamepad buttons: bit index of the frame block's gamepad masks (platform::gamepad_button).
/// </summary>
public enum GamepadButton : byte
{
    A = 0,
    B,
    X,
    Y,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    Guide,
    LeftThumb,
    RightThumb,
    DpadUp,
    DpadRight,
    DpadDown,
    DpadLeft,
}

/// <summary>
/// Gamepad axes (platform::gamepad_axis); triggers are reported in [0, 1].
/// </summary>
public enum GamepadAxis : byte
{
    LeftX = 0,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
}
